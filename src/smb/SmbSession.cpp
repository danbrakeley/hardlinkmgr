#include "SmbSession.h"

#include <QHostAddress>
#include <QHostInfo>
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

// cb_data for an in-flight opendir; owned by m_pendingLists until its
// callback runs (or teardown frees the leftovers).
struct SmbSession::ListRequest
{
    SmbSession *session;
    QString path;
};

// cb_data for an in-flight stat; also owns the result buffer libsmb2 fills.
struct SmbSession::StatRequest
{
    SmbSession *session;
    QString path;
    struct smb2_stat_64 st;
};

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
    spec.host = url.host();
    spec.port = url.port();

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

    // Debug throttle for manual testing (docs/testing.md): a fast LAN server
    // answers stats too quickly to observe the lazy fill-in, so this delays
    // each stat by N ms before it is sent, simulating a slow server. Each
    // delayed stat keeps holding its slot in the view's in-flight window, so
    // throughput becomes ~(window / delay) stats per second.
    m_statDelayMs = qEnvironmentVariableIntValue("HLM_STAT_DELAY_MS");
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

    m_spec = spec;
    m_password = password;
    setState(State::Connecting);

    // Resolve the hostname with Qt's async lookup before handing it to
    // libsmb2: smb2_connect_share_async resolves synchronously (getaddrinfo),
    // which would freeze the UI — unabortably — on slow or failing DNS.
    // Giving it a numeric address makes its own resolution instant.
    if (QHostAddress address; address.setAddress(spec.host)) {
        startSmbConnect(spec.hostPort());
    } else {
        m_lookupId = QHostInfo::lookupHost(spec.host, this, &SmbSession::onHostResolved);
    }
}

void SmbSession::onHostResolved(const QHostInfo &info)
{
    m_lookupId = -1;
    if (m_state != State::Connecting || m_ctx) {
        return; // aborted or otherwise stale
    }

    if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
        const QString error = tr("Could not resolve host \"%1\": %2")
                                  .arg(m_spec.host, info.errorString());
        m_password.clear();
        setState(State::Disconnected);
        emit errorOccurred(error);
        return;
    }

    // Pick one address, preferring IPv4. (This forgoes libsmb2's own
    // multi-address fallback, which is fine for the LAN hosts this targets.)
    QHostAddress chosen = info.addresses().first();
    for (const QHostAddress &address : info.addresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol) {
            chosen = address;
            break;
        }
    }

    QString server = chosen.toString();
    if (server.contains(QLatin1Char(':'))) {
        server = QLatin1Char('[') + server + QLatin1Char(']'); // IPv6 literal
    }
    if (m_spec.port != -1) {
        server += QLatin1Char(':') + QString::number(m_spec.port);
    }
    startSmbConnect(server);
}

void SmbSession::startSmbConnect(const QString &server)
{
    m_ctx = smb2_init_context();
    if (!m_ctx) {
        m_password.clear();
        setState(State::Disconnected);
        emit errorOccurred(tr("Failed to initialize the SMB client context."));
        return;
    }

    smb2_set_security_mode(m_ctx, SMB2_NEGOTIATE_SIGNING_ENABLED);
    smb2_set_timeout(m_ctx, kPduTimeoutSeconds);
    if (!m_spec.domain.isEmpty()) {
        smb2_set_domain(m_ctx, m_spec.domain.toUtf8().constData());
    }
    // Always set a password, even an empty one: auth is NTLMSSP (Kerberos is
    // compiled out), and it needs a password value for password-less shares too.
    smb2_set_password(m_ctx, m_password.toUtf8().constData());
    m_password.clear();

    if (smb2_connect_share_async(m_ctx,
                                 server.toUtf8().constData(),
                                 m_spec.share.toUtf8().constData(),
                                 m_spec.user.toUtf8().constData(),
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

void SmbSession::listDirectory(const QString &path)
{
    if (m_state != State::Connected) {
        emit directoryListFailed(path, tr("Not connected."));
        return;
    }

    // libsmb2 takes share-relative paths with no leading slash ("" = root).
    QString smbPath = path;
    while (smbPath.startsWith(QLatin1Char('/'))) {
        smbPath.remove(0, 1);
    }

    auto *request = new ListRequest{this, path};
    if (smb2_opendir_async(m_ctx, smbPath.toUtf8().constData(),
                           &SmbSession::onOpendirDone, request) != 0) {
        delete request;
        emit directoryListFailed(path, tr("Could not list %1: %2")
                                           .arg(path, QString::fromUtf8(smb2_get_error(m_ctx))));
        return;
    }
    m_pendingLists.insert(request);
    syncNotifiersToContext(); // the new PDU usually wants POLLOUT
}

void SmbSession::onOpendirDone(struct smb2_context *ctx, int status,
                               void *commandData, void *privateData)
{
    auto *request = static_cast<ListRequest *>(privateData);
    SmbSession *self = request->session;
    const QString path = request->path;
    self->m_pendingLists.remove(request);
    delete request;

    auto *dir = static_cast<struct smb2dir *>(status == 0 ? commandData : nullptr);

    // During teardown, smb2_destroy_context() flushes pending callbacks with
    // SMB2_STATUS_SHUTDOWN; just release resources, don't touch state.
    if (self->m_inTeardown) {
        if (dir) {
            smb2_closedir(ctx, dir);
        }
        return;
    }

    if (!dir) {
        QString detail = QString::fromUtf8(smb2_get_error(ctx));
        if (detail.isEmpty()) {
            detail = QString::fromLocal8Bit(std::strerror(-status));
        }
        emit self->directoryListFailed(path, tr("Could not list %1: %2").arg(path, detail));
        return;
    }

    QList<FileEntry> entries;
    while (struct smb2dirent *ent = smb2_readdir(ctx, dir)) {
        const QString name = QString::fromUtf8(ent->name);
        if (name == QLatin1String(".") || name == QLatin1String("..")) {
            continue;
        }
        FileEntry entry;
        entry.name = name;
        entry.isDir = (ent->st.smb2_type == SMB2_TYPE_DIRECTORY);
        entry.size = ent->st.smb2_size;
        entry.modified = QDateTime::fromSecsSinceEpoch(qint64(ent->st.smb2_mtime));
        entry.inode = ent->st.smb2_ino;
        // entry.nlink stays unknown: enumeration never reports it (see CLAUDE.md).
        entries.append(entry);
    }
    smb2_closedir(ctx, dir);

    emit self->directoryListed(path, entries);
}

void SmbSession::statFile(const QString &path)
{
    if (m_state != State::Connected) {
        emit statFailed(path, tr("Not connected."));
        return;
    }
    if (m_statDelayMs > 0) {
        QTimer::singleShot(m_statDelayMs, this, [this, path] { statFileNow(path); });
        return;
    }
    statFileNow(path);
}

void SmbSession::statFileNow(const QString &path)
{
    if (m_state != State::Connected) {
        // Can happen when a throttled stat fires after a disconnect. The view
        // has already dropped its in-flight map by then; this reply is ignored.
        emit statFailed(path, tr("Not connected."));
        return;
    }

    QString smbPath = path;
    while (smbPath.startsWith(QLatin1Char('/'))) {
        smbPath.remove(0, 1);
    }

    auto *request = new StatRequest{this, path, {}};
    if (smb2_stat_async(m_ctx, smbPath.toUtf8().constData(), &request->st,
                        &SmbSession::onStatDone, request) != 0) {
        const QString detail = QString::fromUtf8(smb2_get_error(m_ctx));
        delete request;
        emit statFailed(path, detail);
        return;
    }
    m_pendingStats.insert(request);
    syncNotifiersToContext(); // the new PDU usually wants POLLOUT
}

void SmbSession::onStatDone(struct smb2_context *ctx, int status,
                            void * /*commandData*/, void *privateData)
{
    auto *request = static_cast<StatRequest *>(privateData);
    SmbSession *self = request->session;
    const QString path = request->path;
    const struct smb2_stat_64 st = request->st;
    self->m_pendingStats.remove(request);
    delete request;

    if (self->m_inTeardown) {
        return; // flushed by smb2_destroy_context
    }

    if (status != 0) {
        QString detail = QString::fromUtf8(smb2_get_error(ctx));
        if (detail.isEmpty()) {
            detail = QString::fromLocal8Bit(std::strerror(-status));
        }
        emit self->statFailed(path, detail);
        return;
    }

    emit self->fileStatted(path, int(st.smb2_nlink), st.smb2_ino);
}

void SmbSession::onConnectDone(struct smb2_context *ctx, int status,
                               void * /*commandData*/, void *privateData)
{
    auto *self = static_cast<SmbSession *>(privateData);
    if (self->m_inTeardown) {
        return; // flushed by smb2_destroy_context; state is already handled
    }
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
    if (m_lookupId != -1) {
        QHostInfo::abortHostLookup(m_lookupId);
        m_lookupId = -1;
    }
    m_password.clear();
    m_tickTimer->stop();
    delete m_readNotifier;
    m_readNotifier = nullptr;
    delete m_writeNotifier;
    m_writeNotifier = nullptr;
    m_fd = -1;
    if (m_ctx) {
        // Destroying the context flushes still-pending callbacks with
        // SMB2_STATUS_SHUTDOWN; m_inTeardown tells them to only release their
        // resources and leave the session state alone.
        m_inTeardown = true;
        smb2_destroy_context(m_ctx);
        m_inTeardown = false;
        m_ctx = nullptr;
    }
    // A callback flushed above may have set these; left stale, they would tear
    // down the *next* connection on its first service() pass.
    m_teardownPending = false;
    m_pendingError.clear();
    // The flush should have consumed every pending request; don't leak if not.
    qDeleteAll(m_pendingLists);
    m_pendingLists.clear();
    qDeleteAll(m_pendingStats);
    m_pendingStats.clear();
}

void SmbSession::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged(state);
}
