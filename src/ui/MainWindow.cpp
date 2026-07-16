#include "MainWindow.h"

#include <QAction>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Hard Link Manager"));

    m_session = new SmbSession(this);
    connect(m_session, &SmbSession::stateChanged,
            this, &MainWindow::onSessionStateChanged);
    connect(m_session, &SmbSession::errorOccurred,
            this, &MainWindow::onSessionError);

    m_toolBar = addToolBar(tr("Main"));
    m_toolBar->setMovable(false);
    m_toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(QStringLiteral("smb://user@host:port/share"));
    m_urlEdit->setClearButtonEnabled(true);
    m_urlEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolBar->addWidget(m_urlEdit);

    m_connectAction = m_toolBar->addAction(QString());
    connect(m_connectAction, &QAction::triggered,
            this, &MainWindow::onConnectActionTriggered);
    connect(m_urlEdit, &QLineEdit::returnPressed, this, [this] {
        if (m_session->state() == SmbSession::State::Disconnected) {
            m_connectAction->trigger();
        }
    });

    m_toolBar->addSeparator();

    m_linkAction = m_toolBar->addAction(tr("Link"));
    m_linkAction->setEnabled(false); // enabled by cross-view selection logic in milestone 4
    m_linkAction->setToolTip(tr("Replace selected files with hard links (select at least two files)"));

    m_spinnerTimer = new QTimer(this);
    m_spinnerTimer->setInterval(80);
    connect(m_spinnerTimer, &QTimer::timeout, this, &MainWindow::advanceSpinner);

    m_centralLabel = new QLabel(this);
    m_centralLabel->setAlignment(Qt::AlignCenter);
    m_centralLabel->setEnabled(false); // renders the placeholder text greyed out
    setCentralWidget(m_centralLabel);

    statusBar(); // create it up front so messages have somewhere to go
    onSessionStateChanged(SmbSession::State::Disconnected);
    resize(900, 600);
}

void MainWindow::onConnectActionTriggered()
{
    switch (m_session->state()) {
    case SmbSession::State::Disconnected: {
        const QString text = m_urlEdit->text().trimmed();
        if (text.isEmpty()) {
            statusBar()->showMessage(tr("Enter a share URL first, e.g. smb://user@host/share"));
            m_urlEdit->setFocus();
            return;
        }
        QString error;
        const auto spec = SmbShareSpec::fromUrl(QUrl(text), &error);
        if (!spec) {
            statusBar()->showMessage(error);
            m_urlEdit->setFocus();
            return;
        }
        bool ok = false;
        const QString password = QInputDialog::getText(
            this, tr("Connect to SMB Share"),
            tr("Password for %1\n(leave empty if none is needed):").arg(spec->displayName()),
            QLineEdit::Password, QString(), &ok);
        if (!ok) {
            return;
        }
        m_shareDisplayName = spec->displayName();
        m_session->connectToShare(*spec, password);
        break;
    }
    case SmbSession::State::Connecting:
        m_session->abortConnect();
        break;
    case SmbSession::State::Connected:
        m_session->disconnectFromShare();
        break;
    }
}

void MainWindow::onSessionStateChanged(SmbSession::State state)
{
    switch (state) {
    case SmbSession::State::Disconnected:
        m_spinnerTimer->stop();
        m_connectAction->setText(tr("Connect"));
        m_connectAction->setToolTip(tr("Connect to the SMB share"));
        m_connectAction->setIcon(style()->standardIcon(QStyle::SP_DriveNetIcon));
        m_urlEdit->setEnabled(true);
        m_centralLabel->setText(tr("Not connected.\nEnter smb://user@host:port/share and press Connect."));
        statusBar()->showMessage(tr("Disconnected."));
        break;
    case SmbSession::State::Connecting:
        m_spinnerAngle = 0;
        advanceSpinner();
        m_spinnerTimer->start();
        m_connectAction->setText(tr("Abort"));
        m_connectAction->setToolTip(tr("Abort the connection attempt"));
        m_urlEdit->setEnabled(false);
        m_centralLabel->setText(tr("Connecting to %1…").arg(m_shareDisplayName));
        statusBar()->showMessage(tr("Connecting to %1…").arg(m_shareDisplayName));
        break;
    case SmbSession::State::Connected:
        m_spinnerTimer->stop();
        m_connectAction->setText(tr("Disconnect"));
        m_connectAction->setToolTip(tr("Disconnect from the share"));
        m_connectAction->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
        m_urlEdit->setEnabled(false);
        m_centralLabel->setText(tr("Connected to %1.\n(File browser views arrive in milestone 2.)")
                                    .arg(m_shareDisplayName));
        statusBar()->showMessage(tr("Connected to %1.").arg(m_shareDisplayName));
        break;
    }
}

// The session emits stateChanged(Disconnected) before errorOccurred(), so the
// error lands after (and overwrites) the plain "Disconnected." message.
void MainWindow::onSessionError(const QString &message)
{
    statusBar()->showMessage(message);
}

void MainWindow::advanceSpinner()
{
    m_spinnerAngle = (m_spinnerAngle + 30) % 360;
    m_connectAction->setIcon(QIcon(spinnerPixmap(m_spinnerAngle)));
}

// A simple painted spinner (rotating open arc) so the connecting state needs
// no icon assets.
QPixmap MainWindow::spinnerPixmap(int angleDegrees) const
{
    const QSize logical = m_toolBar->iconSize();
    const qreal dpr = devicePixelRatioF();
    QPixmap pixmap(logical * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    const qreal penWidth = qMax<qreal>(2.0, logical.width() / 8.0);
    QRectF arcRect(QPointF(0, 0), QSizeF(logical));
    const qreal margin = penWidth / 2.0 + 1.0;
    arcRect.adjust(margin, margin, -margin, -margin);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(palette().color(QPalette::Highlight), penWidth,
                        Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(arcRect, -angleDegrees * 16, 300 * 16);
    return pixmap;
}
