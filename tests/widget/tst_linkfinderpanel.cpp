#include <QtTest>

#include <QAbstractProxyModel>
#include <QCheckBox>
#include <QComboBox>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTreeView>

#include "models/LinkResultsModel.h"
#include "smb/SmbSession.h"
#include "ui/LinkFinderPanel.h"

#include "common/SmbFixture.h"
#include "common/TestMain.h"

// The Link Finder panel driven through its widgets: search + browse only, no
// linking action. The traversal/grouping engine has its own suite
// (tst_linksearcher); the pure grouping math has tst_linkgrouping.
class TestLinkFinderPanel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void basicSearchShowsResults();
    void cancelRevertsButton();
    void badPathFailsAtStartSearch();
    void selectingRowEmitsReveal();
    void optionsPersist();

private:
    static LinkResultsModel *resultsModel(LinkFinderPanel &panel);

    SmbFixture m_fx;
};

void TestLinkFinderPanel::initTestCase()
{
    SmbSession session;
    HLM_CONNECT_OR_SKIP(m_fx, session);
}

void TestLinkFinderPanel::init()
{
    QSettings().clear(); // options load in the ctor; start each test clean
}

LinkResultsModel *TestLinkFinderPanel::resultsModel(LinkFinderPanel &panel)
{
    return qobject_cast<LinkResultsModel *>(
        static_cast<QAbstractProxyModel *>(
            panel.findChild<QTreeView *>("lfp.tree")->model())
            ->sourceModel());
}

void TestLinkFinderPanel::basicSearchShowsResults()
{
    const QString dir = m_fx.makeCaseDir("panelbasic");
    m_fx.seedFile(dir, "one.bin", 1000);
    m_fx.makeHardLink(dir + "/one.bin", dir + "/two.bin");
    m_fx.seedFile(dir, "alone.bin", 1000);

    SmbSession session;
    QVERIFY(m_fx.connectForTest(session));
    LinkFinderPanel panel(&session);
    panel.show();

    panel.findChild<QLineEdit *>("lfp.searchPath")->setText(dir);
    panel.findChild<QPushButton *>("lfp.startButton")->click();

    QTRY_COMPARE(resultsModel(panel)->rowCount(), 2);
    QTRY_VERIFY(panel.findChild<QLabel *>("lfp.resultsLabel")->text().contains("2 result(s)."));
    QCOMPARE(panel.findChild<QPushButton *>("lfp.startButton")->text(), "Start Search");
}

void TestLinkFinderPanel::cancelRevertsButton()
{
    const QString dir = m_fx.makeCaseDir("panelcancel");
    m_fx.seedManyDirs(dir, 20, 64);

    SmbSession session;
    QVERIFY(m_fx.connectForTest(session));
    LinkFinderPanel panel(&session);
    panel.show();

    panel.findChild<QLineEdit *>("lfp.searchPath")->setText(dir);
    auto *startButton = panel.findChild<QPushButton *>("lfp.startButton");
    startButton->click();
    // Everything is single-threaded: the search cannot progress between
    // these two clicks, so the cancel path is fully deterministic.
    QCOMPARE(startButton->text(), "Cancel Search");

    startButton->click(); // cancel
    QCOMPARE(startButton->text(), "Start Search");
    QCOMPARE(panel.findChild<QLabel *>("lfp.statusLabel")->text(), "Search cancelled.");

    // Late listing replies must not resurrect anything.
    QTest::qWait(500);
    QCOMPARE(startButton->text(), "Start Search");
    QCOMPARE(resultsModel(panel)->rowCount(), 0);
}

// Path validation happens at Start-Search time (not on connect, unlike Match
// Finder): a bad path fails the search cleanly instead of a separate
// pre-validation step.
void TestLinkFinderPanel::badPathFailsAtStartSearch()
{
    SmbSession session;
    QVERIFY(m_fx.connectForTest(session));
    LinkFinderPanel panel(&session);
    panel.show();

    panel.findChild<QLineEdit *>("lfp.searchPath")->setText("/no-such-dir-anywhere");
    panel.findChild<QPushButton *>("lfp.startButton")->click();

    QTRY_VERIFY(panel.findChild<QLabel *>("lfp.statusLabel")
                    ->text().contains("/no-such-dir-anywhere"));
    QCOMPARE(panel.findChild<QPushButton *>("lfp.startButton")->text(), "Start Search");
    QCOMPARE(resultsModel(panel)->rowCount(), 0);
}

void TestLinkFinderPanel::selectingRowEmitsReveal()
{
    const QString dir = m_fx.makeCaseDir("panelreveal");
    m_fx.seedFile(dir, "one.bin", 1000);
    m_fx.makeHardLink(dir + "/one.bin", dir + "/two.bin");

    SmbSession session;
    QVERIFY(m_fx.connectForTest(session));
    LinkFinderPanel panel(&session);
    panel.show();
    QSignalSpy revealSpy(&panel, &LinkFinderPanel::revealRequested);

    panel.findChild<QLineEdit *>("lfp.searchPath")->setText(dir);
    panel.findChild<QPushButton *>("lfp.startButton")->click();
    QTRY_COMPARE(resultsModel(panel)->rowCount(), 2);

    auto *tree = panel.findChild<QTreeView *>("lfp.tree");
    tree->selectionModel()->setCurrentIndex(
        tree->model()->index(0, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    QCOMPARE(revealSpy.count(), 1);
    QCOMPARE(revealSpy.first().at(0).toString(), dir);
    QVERIFY(revealSpy.first().at(1).toString() == "one.bin"
            || revealSpy.first().at(1).toString() == "two.bin");
}

void TestLinkFinderPanel::optionsPersist()
{
    SmbSession session; // never needs to connect for this one

    {
        LinkFinderPanel panel(&session);
        auto *searchPath = panel.findChild<QLineEdit *>("lfp.searchPath");
        searchPath->setText("/media/movies");
        QTest::keyClick(searchPath, Qt::Key_Return); // editingFinished -> saved
        panel.findChild<QCheckBox *>("lfp.recurse")->setChecked(false);
        panel.findChild<QSpinBox *>("lfp.sizeMinValue")->setValue(25);
        panel.findChild<QComboBox *>("lfp.sizeMinUnit")->setCurrentIndex(2); // MiB
        panel.findChild<QSpinBox *>("lfp.linkMinValue")->setValue(5);
    }

    LinkFinderPanel reopened(&session);
    QCOMPARE(reopened.findChild<QLineEdit *>("lfp.searchPath")->text(), "/media/movies");
    QVERIFY(!reopened.findChild<QCheckBox *>("lfp.recurse")->isChecked());
    QCOMPARE(reopened.findChild<QSpinBox *>("lfp.sizeMinValue")->value(), 25);
    QCOMPARE(reopened.findChild<QComboBox *>("lfp.sizeMinUnit")->currentIndex(), 2);
    QCOMPARE(reopened.findChild<QSpinBox *>("lfp.linkMinValue")->value(), 5);
}

HLM_TEST_MAIN(TestLinkFinderPanel)

#include "tst_linkfinderpanel.moc"
