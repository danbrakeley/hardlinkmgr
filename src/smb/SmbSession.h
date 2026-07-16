#pragma once

#include <QObject>
#include <QString>

#include <optional>

class QHostInfo;
class QSocketNotifier;
class QTimer;
class QUrl;

struct smb2_context;

// The parsed pieces of a `smb://[domain;]user@host[:port]/share` URL.
struct SmbShareSpec
{
    QString host;
    int port = -1;   // -1 means the SMB default (445)
    QString share;   // share name (no slashes)
    QString user;
    QString domain;  // optional workgroup/domain (from the "domain;user" URL syntax)

    // "host", "host:port", or "[v6addr]:port" — the form libsmb2 expects.
    QString hostPort() const
    {
        QString h = host.contains(QLatin1Char(':'))
                        ? QLatin1Char('[') + host + QLatin1Char(']')
                        : host;
        return port != -1 ? h + QLatin1Char(':') + QString::number(port) : h;
    }

    QString displayName() const
    {
        return user + QLatin1Char('@') + hostPort() + QLatin1Char('/') + share;
    }

    // Returns std::nullopt and fills *errorMessage if the URL is not a valid
    // share-root SMB URL.
    static std::optional<SmbShareSpec> fromUrl(const QUrl &url, QString *errorMessage);
};

// Asynchronous wrapper around a single libsmb2 connection.
//
// Runs entirely on the GUI thread: libsmb2's async API is driven by
// QSocketNotifiers on the context's socket plus a coarse tick timer for its
// timeout processing, so the rest of the app needs no locking or cross-thread
// marshalling. This is also what makes the connect flow abortable and (from
// milestone 3 on) lets many stat requests be in flight at once.
class SmbSession : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        Connecting,
        Connected,
    };
    Q_ENUM(State)

    explicit SmbSession(QObject *parent = nullptr);
    ~SmbSession() override;

    State state() const { return m_state; }

    // Begins connecting. Progress is reported via stateChanged(); a failed
    // attempt emits stateChanged(Disconnected) followed by errorOccurred().
    // No-op unless currently Disconnected.
    void connectToShare(const SmbShareSpec &spec, const QString &password);

    // Cancels an in-progress connection attempt (Connecting -> Disconnected).
    void abortConnect();

    // Drops an established connection (Connected -> Disconnected).
    void disconnectFromShare();

signals:
    void stateChanged(SmbSession::State state);
    void errorOccurred(const QString &message);

private:
    static void onConnectDone(struct smb2_context *ctx, int status,
                              void *commandData, void *privateData);

    void onHostResolved(const QHostInfo &info);
    void startSmbConnect(const QString &server);
    void service(int revents);
    void syncNotifiersToContext();
    void teardown();
    void setState(State state);

    SmbShareSpec m_spec;    // spec of the current/last connection attempt
    QString m_password;     // held only between connectToShare() and startSmbConnect()
    int m_lookupId = -1;    // pending QHostInfo lookup, -1 when none
    struct smb2_context *m_ctx = nullptr;
    QSocketNotifier *m_readNotifier = nullptr;
    QSocketNotifier *m_writeNotifier = nullptr;
    QTimer *m_tickTimer = nullptr;
    qintptr m_fd = -1;
    State m_state = State::Disconnected;
    bool m_teardownPending = false;
    QString m_pendingError;
};
