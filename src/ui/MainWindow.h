#pragma once

#include <QMainWindow>
#include <QPixmap>

#include "smb/SmbSession.h"

class QAction;
class QLabel;
class QLineEdit;
class QSplitter;
class QTimer;
class QToolBar;

class FileBrowserView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void onConnectActionTriggered();
    void onLinkActionTriggered();
    void updateLinkAction();
    void addView();
    void removeView(FileBrowserView *view);
    void onSessionStateChanged(SmbSession::State state);
    void onSessionError(const QString &message);
    void advanceSpinner();
    QPixmap spinnerPixmap(int angleDegrees) const;

    SmbSession *m_session = nullptr;
    QToolBar *m_toolBar = nullptr;
    QLineEdit *m_urlEdit = nullptr;
    QAction *m_connectAction = nullptr;
    QAction *m_linkAction = nullptr;
    QAction *m_addViewAction = nullptr;
    QLabel *m_centralLabel = nullptr;         // placeholder while not connected
    QSplitter *m_splitter = nullptr;          // central widget while connected
    QList<FileBrowserView *> m_views;         // children of m_splitter
    QTimer *m_spinnerTimer = nullptr;
    int m_spinnerAngle = 0;
    QString m_shareDisplayName; // user@host/share of the current/last attempt
};
