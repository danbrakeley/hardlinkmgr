#include "SmbSession.h"

#include <QSocketNotifier>
#include <QTimer>
#include <QUrl>

#include <smb2/smb2.h>      // must precede libsmb2.h: defines SMB2_GUID_SIZE, smb2_lease_key, etc.
#include <smb2/libsmb2.h>

#ifdef Q_OS_UNIX
#include <poll.h>           // POLLIN/POLLOUT; on Windows winsock2.h (pulled in by libsmb2.h) provides them
#endif

#include <cstring>

namespace {

// With smb2_set_timeout in effect, libsmb2 needs smb2_service() called at
// least once per second to notice expired PDUs; the tick also picks up fd
// changes that happen without a socket event (e.g. multi-address connects).
constexpr int kTickIntervalMs = 250;

// Per-PDU timeout, which also bounds the SMB negotiation phase of a connect.
// (The TCP phase is bounded by the OS; the user can always Abort.)
constexpr int kPduTimeoutSeconds = 15;

} // namespace

std::optional<SmbShareSpec> SmbShareSpec::fromUrl(const QUrl &url, QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &msg) -> std::optional<SmbShareSpec> {
        if (errorMessage) {
            *errorMessage = msg;
        }
        return std::nullopt;
    };

    if (!url.isValid()) {
        return fail(QObject::tr("Invalid URL: %1").arg(url.errorString()));
    }
    if (url.scheme() != QLatin1String("smb")) {
        return fail(QObject::tr("The URL must start with smb://"));
    }
    if (url.host().isEmpty()) {
        return fail(QObject::tr("The URL is missing a host; expected smb://user@host:port/share"));
    }

    SmbShareSpec spec;
    spec.server = url.host();
    if (url.port() != -1) {
        spec.server += QLatin1Char(':') + QString::number(url.port());
    }

    spec.user = url.userName();
    if (const int sep = spec.user.indexOf(QLatin1Char(';')); sep >= 0) {
        spec.domain = spec.user.left(sep);
        spec.user = spec.user.mid(sep + 1);
    }
    if (spec.user.isEmpty()) {
        return fail(QObject::tr("The URL is missing a user; expected smb://user@host:port/share"));
    }

    QString share = url.path();
    while (share.startsWith(QLatin1Char('/'))) {
        share.remove(0, 1);
    }
    while (share.endsWith(QLatin1Char('/'))) {
        share.chop(1);
    }
    if (share.isEmpty()) {
        return fail(QObject::tr("The URL is missing a share name; expected smb://user@host:port/share"));
    }
    if (share.contains(QLatin1Char('/'))) {
        return fail(QObject::tr("The URL must name the share root (no path after the share name)"));
    }
    spec.share = share;

    return spec;
}

SmbSession::SmbSession(QObject *parent)
    : QObject(parent)
{
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(kTickIntervalMs);
    connect(m_tickTimer, &QTimer::timeout, this, [this] { service(0); });
}

SmbSession::~SmbSession()
{
    teardown();
}

void SmbSession::connectToShare(const SmbShareSpec &spec, const QString &password)
{
    if (m_state != State::Disconnected) {
        return;
    }

    m_ctx = smb2_init_context();
    if (!m_ctx) {
        emit errorOccurred(tr("Failed to initialize the SMB client context."));
        return;
    }

    smb2_set_security_mode(m_ctx, SMB2_NEGOTIATE_SIGNING_ENABLED);
    smb2_set_timeout(m_ctx, kPduTimeoutSeconds);
    if (!spec.domain.isEmpty()) {
        smb2_set_domain(m_ctx, spec.domain.toUtf8().constData());
    }
    // Always set a password, even an empty one: auth is NTLMSSP (Kerberos is
    // compiled out), and it needs a password value for password-less shares too.
    smb2_set_password(m_ctx, password.toUtf8().constData());

    setState(State::Connecting);

    // Note: name resolution inside this call is synchronous, so a slow DNS
    // server can stall the UI briefly. Revisit if it becomes noticeable.
    if (smb2_connect_share_async(m_ctx,
                                 spec.server.toUtf8().constData(),
                                 spec.share.toUtf8().constData(),
                                 spec.user.toUtf8().constData(),
                                 &SmbSession::onConnectDone, this) != 0) {
        const QString error = tr("Connection failed: %1")
                                  .arg(QString::fromUtf8(smb2_get_error(m_ctx)));
        teardown();
        setState(State::Disconnected);
        emit errorOccurred(error);
        return;
    }

    m_tickTimer->start();
    syncNotifiersToContext();
}

void SmbSession::abortConnect()
{
    if (m_state != State::Connecting) {
        return;
    }
    teardown();
    setState(State::Disconnected);
}

// Abrupt but sufficient: closing the TCP connection is the fallback cleanup
// path of every SMB server, which reaps the session. A polite
// LOGOFF/TREE_DISCONNECT exchange can be added later if it ever matters.
void SmbSession::disconnectFromShare()
{
    if (m_state != State::Connected) {
        return;
    }
    teardown();
    setState(State::Disconnected);
}

void SmbSession::onConnectDone(struct smb2_context *ctx, int status,
                               void * /*commandData*/, void *privateData)
{
    auto *self = static_cast<SmbSession *>(privateData);
    if (status < 0) {
        QString detail = QString::fromUtf8(smb2_get_error(ctx));
        if (detail.isEmpty()) {
            detail = QString::fromLocal8Bit(std::strerror(-status));
        }
        // We are inside smb2_service() here, so the context must not be
        // destroyed yet; record the failure and let service() finish up.
        self->m_pendingError = tr("Connection failed: %1").arg(detail);
        self->m_teardownPending = true;
        return;
    }
    self->setState(State::Connected);
}

void SmbSession::service(int revents)
{
    if (!m_ctx) {
        return;
    }

    if (smb2_service(m_ctx, revents) < 0) {
        if (m_pendingError.isEmpty()) {
            m_pendingError = tr("SMB connection error: %1")
                                 .arg(QString::fromUtf8(smb2_get_error(m_ctx)));
        }
        m_teardownPending = true;
    }

    if (m_teardownPending) {
        const QString error = m_pendingError;
        m_teardownPending = false;
        m_pendingError.clear();
        teardown();
        setState(State::Disconnected);
        if (!error.isEmpty()) {
            emit errorOccurred(error);
        }
        return;
    }

    syncNotifiersToContext();
}

void SmbSession::syncNotifiersToContext()
{
    if (!m_ctx) {
        return;
    }

    // The fd can change across service calls (reconnects, multi-address
    // connects), so re-check it every time, as the libsmb2 docs recommend.
    const auto fd = static_cast<qintptr>(smb2_get_fd(m_ctx));
    if (fd != m_fd) {
        delete m_readNotifier;
        m_readNotifier = nullptr;
        delete m_writeNotifier;
        m_writeNotifier = nullptr;
        m_fd = fd;
        if (fd != -1) { // -1 is SMB2_INVALID_SOCKET / INVALID_SOCKET cast to qintptr
            m_readNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            connect(m_readNotifier, &QSocketNotifier::activated,
                    this, [this] { service(POLLIN); });
            m_writeNotifier = new QSocketNotifier(fd, QSocketNotifier::Write, this);
            connect(m_writeNotifier, &QSocketNotifier::activated,
                    this, [this] { service(POLLOUT); });
        }
    }
    if (!m_readNotifier) {
        return;
    }

    const int events = smb2_which_events(m_ctx);
    m_readNotifier->setEnabled(events & POLLIN);
    m_writeNotifier->setEnabled(events & POLLOUT);
}

// Frees the context and stops event delivery. Must never run while inside
// smb2_service() — code running under a libsmb2 callback sets
// m_teardownPending instead, and service() finishes the job afterwards.
void SmbSession::teardown()
{
    m_tickTimer->stop();
    delete m_readNotifier;
    m_readNotifier = nullptr;
    delete m_writeNotifier;
    m_writeNotifier = nullptr;
    m_fd = -1;
    if (m_ctx) {
        smb2_destroy_context(m_ctx);
        m_ctx = nullptr;
    }
}

void SmbSession::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged(state);
}
