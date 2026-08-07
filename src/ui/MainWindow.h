#pragma once

#include <QMainWindow>
#include <QPixmap>

#include <functional>
#include <optional>

#include "models/FileListModel.h" // for the nested IconMode enum
#include "smb/SmbSession.h"

class QAction;
class QCloseEvent;
class QLabel;
class QLineEdit;
class QSplitter;
class QTabWidget;
class QTimer;
class QToolBar;

class FileBrowserView;
class LinkFinderPanel;
class MatchFinderPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // How the connect flow asks for the password; std::nullopt = cancelled.
    // The default shows the modal QInputDialog; tests inject a stub so no
    // dialog ever blocks a headless run.
    using PasswordPrompt =
        std::function<std::optional<QString>(const QString &shareDisplayName)>;
    void setPasswordPrompt(PasswordPrompt prompt);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void restoreWindowGeometry();
    void saveSplitterState();
    void onConnectActionTriggered();
    void onAboutActionTriggered();
    FileBrowserView *createView();
    void addView();
    void onRevealRequested(const QString &primaryFolder, const QString &primaryName,
                           const QString &secondaryFolder, const QString &secondaryName);
    void onLinkFinderRevealRequested(const QString &folder, const QString &name);
    void updateStatsPaused(); // pauses every view's lazy stat pump while either search runs
    void onSessionStateChanged(SmbSession::State state);
    void onSessionError(const QString &message);
    FileListModel::IconMode currentIconMode() const;
    void onIconModeChangeRequested(FileListModel::IconMode mode);
    void advanceSpinner();
    QPixmap spinnerPixmap(int angleDegrees) const;

    SmbSession *m_session = nullptr;
    QToolBar *m_toolBar = nullptr;
    QLineEdit *m_urlEdit = nullptr;
    QAction *m_connectAction = nullptr;
    QAction *m_aboutAction = nullptr;
    QLabel *m_centralLabel = nullptr;         // placeholder while not connected
    QTabWidget *m_tabWidget = nullptr;        // central widget while connected
    QSplitter *m_hSplitter = nullptr;         // Match Finder tab
    QSplitter *m_splitter = nullptr;          // left side: the stacked views
    MatchFinderPanel *m_matchPanel = nullptr; // right side of m_hSplitter
    QSplitter *m_linkSplitter = nullptr;      // Link Finder tab
    LinkFinderPanel *m_linkPanel = nullptr;   // left side of m_linkSplitter
    FileBrowserView *m_linkView = nullptr;    // right side of m_linkSplitter
    QList<FileBrowserView *> m_views;         // every FileBrowserView in either tab
    bool m_matchSearching = false; // mirrors MatchFinderPanel::searchRunningChanged
    bool m_linkSearching = false;  // mirrors LinkFinderPanel::searchRunningChanged
    QTimer *m_spinnerTimer = nullptr;
    int m_spinnerAngle = 0;
    QString m_shareDisplayName; // user@host/share of the current/last attempt
    PasswordPrompt m_passwordPrompt;
};
