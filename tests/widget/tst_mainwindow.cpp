#include <QtTest>

#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>

#include "ui/FileBrowserView.h"
#include "ui/LinkFinderPanel.h"
#include "ui/MainWindow.h"
#include "ui/MatchFinderPanel.h"

#include "common/SmbFixture.h"
#include "common/TestMain.h"

// The main-window connect flow and toolbar logic, driven through the real
// widgets with a stubbed password prompt (the modal QInputDialog never runs
// under test). Covers testing.md M1 "Connect"/"Disconnect"/"Cancelled
// password prompt"/"Bad URL", M5 "Cross-view selection"/"Remembered URL", the
// M7 panel layout, and the Match Finder / Link Finder tab split.
class TestMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void badUrlShowsErrorWithoutPrompt();
    void emptyUrlAsksForOne();
    void cancelledPromptStaysDisconnected();
    void connectFlowBuildsTabsViewsAndPanels();
    void searchPausesEveryViewsStatPump();
    void disconnectRestoresPlaceholder();
    void rememberedUrl();

private:
    // Installs a prompt stub and drives the toolbar through a full connect.
    void connectWindow(MainWindow &window);

    SmbFixture m_fx;
};

void TestMainWindow::initTestCase()
{
    SmbSession session;
    HLM_CONNECT_OR_SKIP(m_fx, session);
}

void TestMainWindow::init()
{
    QSettings().clear(); // isolated by QStandardPaths test mode (TestMain.h)
}

void TestMainWindow::connectWindow(MainWindow &window)
{
    window.setPasswordPrompt(
        [this](const QString &) { return std::optional<QString>(m_fx.password()); });
    auto *urlEdit = window.findChild<QLineEdit *>("mw.urlEdit");
    auto *connectAction = window.findChild<QAction *>("mw.connectAction");
    QVERIFY(urlEdit && connectAction);
    urlEdit->setText(m_fx.url());
    connectAction->trigger();
    QTRY_COMPARE_WITH_TIMEOUT(connectAction->text(), "Disconnect", 15000);
}

void TestMainWindow::badUrlShowsErrorWithoutPrompt()
{
    MainWindow window;
    bool promptCalled = false;
    window.setPasswordPrompt([&promptCalled](const QString &) {
        promptCalled = true;
        return std::optional<QString>();
    });
    auto *urlEdit = window.findChild<QLineEdit *>("mw.urlEdit");
    auto *connectAction = window.findChild<QAction *>("mw.connectAction");

    urlEdit->setText("smb://host-without-user/share");
    connectAction->trigger();
    QVERIFY(!promptCalled);
    QVERIFY(window.statusBar()->currentMessage().contains("missing a user"));
    QCOMPARE(connectAction->text(), "Connect");

    urlEdit->setText("smb://user@host-without-share");
    connectAction->trigger();
    QVERIFY(!promptCalled);
    QVERIFY(window.statusBar()->currentMessage().contains("missing a share"));
}

void TestMainWindow::emptyUrlAsksForOne()
{
    MainWindow window;
    auto *connectAction = window.findChild<QAction *>("mw.connectAction");
    window.findChild<QLineEdit *>("mw.urlEdit")->clear();
    connectAction->trigger();
    QVERIFY(window.statusBar()->currentMessage().contains("Enter a share URL"));
}

void TestMainWindow::cancelledPromptStaysDisconnected()
{
    MainWindow window;
    window.setPasswordPrompt(
        [](const QString &) { return std::optional<QString>(); }); // = cancel
    auto *urlEdit = window.findChild<QLineEdit *>("mw.urlEdit");
    auto *connectAction = window.findChild<QAction *>("mw.connectAction");

    urlEdit->setText(m_fx.url());
    connectAction->trigger();

    QCOMPARE(connectAction->text(), "Connect"); // no attempt started
    QVERIFY(urlEdit->isEnabled());
    QCOMPARE(window.findChildren<FileBrowserView *>().size(), 0);
}

void TestMainWindow::connectFlowBuildsTabsViewsAndPanels()
{
    MainWindow window;
    connectWindow(window);

    // A Match Finder tab (2 views, ADR 0003, left of the panel) and a Link
    // Finder tab (1 view, right of its panel) — 3 FileBrowserViews total.
    QCOMPARE(window.findChildren<FileBrowserView *>().size(), 3);
    QCOMPARE(window.findChildren<MatchFinderPanel *>().size(), 1);
    QCOMPARE(window.findChildren<LinkFinderPanel *>().size(), 1);

    auto *tabWidget = window.findChild<QTabWidget *>("mw.tabWidget");
    QVERIFY(tabWidget);
    QCOMPARE(tabWidget, window.centralWidget());
    QCOMPARE(tabWidget->count(), 2);
    QCOMPARE(tabWidget->tabText(0), "Match Finder");
    QCOMPARE(tabWidget->tabText(1), "Link Finder");
    QVERIFY(qobject_cast<QSplitter *>(tabWidget->widget(0)));
    QVERIFY(qobject_cast<QSplitter *>(tabWidget->widget(1)));

    QVERIFY(!window.findChild<QLineEdit *>("mw.urlEdit")->isEnabled());
    QVERIFY(window.statusBar()->currentMessage().startsWith("Connected to"));

    // Every view comes up listing the share root.
    for (FileBrowserView *view : window.findChildren<FileBrowserView *>()) {
        QTRY_COMPARE(view->currentPath(), "/");
    }
}

// SmbSession has no worker thread, so a folder's worth of lazy stat calls
// competes with a running search for the same GUI-thread-serviced
// connection; MainWindow pauses every view's pump while either search runs.
void TestMainWindow::searchPausesEveryViewsStatPump()
{
    const QString dir = m_fx.makeCaseDir("statpause");
    m_fx.seedFile(dir, "a.bin", 100);
    m_fx.seedFile(dir, "b.bin", 100);

    MainWindow window;
    connectWindow(window);

    QList<FileBrowserView *> views = window.findChildren<FileBrowserView *>();
    QCOMPARE(views.size(), 3);
    for (FileBrowserView *view : views) {
        QVERIFY(!view->statsPaused());
    }

    auto *linkPanel = window.findChild<LinkFinderPanel *>();
    QSignalSpy runningSpy(linkPanel, &LinkFinderPanel::searchRunningChanged);
    linkPanel->findChild<QLineEdit *>("lfp.searchPath")->setText(dir);
    linkPanel->findChild<QPushButton *>("lfp.startButton")->click();

    QTRY_COMPARE(runningSpy.count(), 1);
    QVERIFY(runningSpy.first().at(0).toBool());
    for (FileBrowserView *view : views) {
        QVERIFY(view->statsPaused());
    }

    QTRY_COMPARE(runningSpy.count(), 2);
    QVERIFY(!runningSpy.last().at(0).toBool());
    for (FileBrowserView *view : views) {
        QVERIFY(!view->statsPaused());
    }
}

void TestMainWindow::disconnectRestoresPlaceholder()
{
    MainWindow window;
    connectWindow(window);
    auto *connectAction = window.findChild<QAction *>("mw.connectAction");

    connectAction->trigger(); // now "Disconnect"

    QCOMPARE(connectAction->text(), "Connect");
    QVERIFY(window.findChild<QLineEdit *>("mw.urlEdit")->isEnabled());
    QVERIFY(!qobject_cast<QSplitter *>(window.centralWidget()));
    // The old splitter (and the views under it) go away via deleteLater.
    QTRY_COMPARE(window.findChildren<FileBrowserView *>().size(), 0);
}

void TestMainWindow::rememberedUrl()
{
    {
        MainWindow window;
        connectWindow(window); // a successful connect stores connect/lastUrl
    }
    MainWindow relaunched;
    QCOMPARE(relaunched.findChild<QLineEdit *>("mw.urlEdit")->text(), m_fx.url());
}

HLM_TEST_MAIN(TestMainWindow)

#include "tst_mainwindow.moc"
