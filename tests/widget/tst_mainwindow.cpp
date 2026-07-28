#include <QtTest>

#include <QLineEdit>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTreeView>

#include "ui/FileBrowserView.h"
#include "ui/MainWindow.h"
#include "ui/MatchFinderPanel.h"

#include "common/SmbFixture.h"
#include "common/TestMain.h"

// The main-window connect flow and toolbar logic, driven through the real
// widgets with a stubbed password prompt (the modal QInputDialog never runs
// under test). Covers testing.md M1 "Connect"/"Disconnect"/"Cancelled
// password prompt"/"Bad URL", M4 "Link enablement", M5 "Cross-view
// selection"/"Remembered URL", and the M7 panel layout.
class TestMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void badUrlShowsErrorWithoutPrompt();
    void emptyUrlAsksForOne();
    void cancelledPromptStaysDisconnected();
    void connectFlowBuildsTwoViewsAndPanel();
    void disconnectRestoresPlaceholder();
    void linkEnablement();
    void rememberedUrl();

private:
    // Installs a prompt stub and drives the toolbar through a full connect.
    void connectWindow(MainWindow &window);
    static void selectByName(FileBrowserView *view, const QString &name,
                             bool addToSelection);

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

void TestMainWindow::selectByName(FileBrowserView *view, const QString &name,
                                  bool addToSelection)
{
    auto *tree = view->findChild<QTreeView *>("fbv.tree");
    QVERIFY(tree);
    QAbstractItemModel *model = tree->model();
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex idx = model->index(row, 1); // NameColumn
        if (idx.data().toString() == name) {
            tree->selectionModel()->select(
                idx, QItemSelectionModel::Rows
                         | (addToSelection ? QItemSelectionModel::Select
                                           : QItemSelectionModel::ClearAndSelect));
            return;
        }
    }
    QFAIL(qPrintable("no row named " + name));
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

void TestMainWindow::connectFlowBuildsTwoViewsAndPanel()
{
    MainWindow window;
    connectWindow(window);

    // Exactly two views (ADR 0003) left of the Match Finder panel.
    QCOMPARE(window.findChildren<FileBrowserView *>().size(), 2);
    QCOMPARE(window.findChildren<MatchFinderPanel *>().size(), 1);
    QVERIFY(qobject_cast<QSplitter *>(window.centralWidget()));
    QVERIFY(!window.findChild<QLineEdit *>("mw.urlEdit")->isEnabled());
    QVERIFY(window.statusBar()->currentMessage().startsWith("Connected to"));

    // Both views come up listing the share root.
    for (FileBrowserView *view : window.findChildren<FileBrowserView *>()) {
        QTRY_COMPARE(view->currentPath(), "/");
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

void TestMainWindow::linkEnablement()
{
    const QString dir = m_fx.makeCaseDir("linkenable");
    m_fx.seedFile(dir, "a.bin", 100);
    m_fx.seedFile(dir, "b.bin", 100);
    m_fx.makeDir(dir + "/folder");

    MainWindow window;
    connectWindow(window);
    auto *linkAction = window.findChild<QAction *>("mw.linkAction");
    const auto views = window.findChildren<FileBrowserView *>();
    QCOMPARE(views.size(), 2);

    views[0]->navigateTo(dir);
    views[1]->navigateTo(dir);
    QTRY_COMPARE(views[0]->currentPath(), dir);
    QTRY_COMPARE(views[1]->currentPath(), dir);
    QVERIFY(!linkAction->isEnabled()); // nothing selected

    selectByName(views[0], "a.bin", false);
    QVERIFY(!linkAction->isEnabled()); // one file

    // A folder does not count toward the two files.
    selectByName(views[0], "folder", true);
    QVERIFY(!linkAction->isEnabled());

    // The same file selected in the other view still counts once.
    selectByName(views[1], "a.bin", false);
    QVERIFY(!linkAction->isEnabled());

    // A second distinct file enables Link (cross-view selection).
    selectByName(views[1], "b.bin", false);
    QVERIFY(linkAction->isEnabled());

    // Navigation clears the selection and disables Link again.
    views[1]->navigateTo("/");
    QTRY_COMPARE(views[1]->currentPath(), "/");
    QVERIFY(!linkAction->isEnabled());
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
