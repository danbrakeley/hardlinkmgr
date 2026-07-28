#include <QtTest>

#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QTreeWidget>

#include "smb/SmbSession.h"
#include "ui/HardLinkDialog.h"

#include "common/SmbFixture.h"
#include "common/TestMain.h"

// The Hard Link dialog driven directly (the LinkRunner engine underneath has
// its own suite). Covers testing.md M4 "Dialog basics" and the UI half of
// "Happy path", and doubles as the M7 "Dialog regression" check. Driven with
// show(), never exec(), so nothing blocks the test loop.
class TestHardLinkDialog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void linkDisabledUntilPrimaryChosen();
    void cancelBeforeRunChangesNothing();
    void happyPathWalksStatuses();

private:
    QList<SelectedFile> seedPair(const QString &caseName, QString *dirOut);

    SmbFixture m_fx;
};

void TestHardLinkDialog::initTestCase()
{
    SmbSession session;
    HLM_CONNECT_OR_SKIP(m_fx, session);
}

// Two throwaway files with the metadata the dialog displays.
QList<SelectedFile> TestHardLinkDialog::seedPair(const QString &caseName, QString *dirOut)
{
    const QString dir = m_fx.makeCaseDir(caseName);
    m_fx.seedFile(dir, "primary.bin", 100, 'p');
    m_fx.seedFile(dir, "victim.bin", 90, 'v');
    if (dirOut) {
        *dirOut = dir;
    }
    QList<SelectedFile> files;
    for (const QString &name : {QStringLiteral("primary.bin"), QStringLiteral("victim.bin")}) {
        const QString path = dir + "/" + name;
        const SmbFixture::Stat st = m_fx.statPath(path);
        FileEntry entry;
        entry.name = name;
        entry.size = name.startsWith("primary") ? 100 : 90;
        entry.modified = QDateTime::currentDateTime();
        entry.inode = st.inode;
        entry.nlink = st.nlink;
        files.append({path, entry});
    }
    return files;
}

void TestHardLinkDialog::linkDisabledUntilPrimaryChosen()
{
    QString dir;
    const auto files = seedPair("gating", &dir);

    SmbSession session;
    QVERIFY(m_fx.connectForTest(session));
    HardLinkDialog dialog(&session, files);
    dialog.show();

    auto *list = dialog.findChild<QTreeWidget *>("hld.list");
    auto *linkButton = dialog.findChild<QPushButton *>("hld.linkButton");
    QCOMPARE(list->topLevelItemCount(), 2); // both files listed
    QVERIFY(!list->topLevelItem(0)->text(1).isEmpty());
    QVERIFY(!linkButton->isEnabled()); // no primary chosen yet

    const auto radios = dialog.findChildren<QRadioButton *>();
    QCOMPARE(radios.size(), 2);
    radios.first()->click();
    QVERIFY(linkButton->isEnabled());
}

void TestHardLinkDialog::cancelBeforeRunChangesNothing()
{
    QString dir;
    const auto files = seedPair("cancel", &dir);

    SmbSession session;
    QVERIFY(m_fx.connectForTest(session));
    HardLinkDialog dialog(&session, files);
    dialog.show();

    dialog.findChild<QPushButton *>("hld.closeButton")->click(); // "Cancel"
    QCOMPARE(dialog.result(), int(QDialog::Rejected));

    // Nothing on the share changed.
    QCOMPARE(m_fx.statPath(dir + "/primary.bin").nlink, 1);
    QCOMPARE(m_fx.statPath(dir + "/victim.bin").nlink, 1);
    QCOMPARE(m_fx.readFile(dir + "/victim.bin"), QString(90, QLatin1Char('v')));
}

void TestHardLinkDialog::happyPathWalksStatuses()
{
    QString dir;
    const auto files = seedPair("happy", &dir);

    SmbSession session;
    QVERIFY(m_fx.connectForTest(session));
    HardLinkDialog dialog(&session, files);
    dialog.show();

    dialog.findChildren<QRadioButton *>().first()->click(); // keep primary.bin
    dialog.findChild<QPushButton *>("hld.linkButton")->click();

    auto *closeButton = dialog.findChild<QPushButton *>("hld.closeButton");
    QTRY_COMPARE_WITH_TIMEOUT(closeButton->text(), "Close", 30000);

    auto *list = dialog.findChild<QTreeWidget *>("hld.list");
    QCOMPARE(list->topLevelItem(0)->text(3), "kept (primary)");
    QCOMPARE(list->topLevelItem(1)->text(3), "replaced with hard link");
    QVERIFY(dialog.findChild<QLabel *>("hld.summaryLabel")
                ->text().startsWith("Replaced 1 file(s)"));

    // Closing after a run reports Accepted so MainWindow refreshes the views.
    closeButton->click();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));

    QCOMPARE(m_fx.statPath(dir + "/primary.bin").nlink, 2);
    QCOMPARE(m_fx.statPath(dir + "/victim.bin").inode,
             m_fx.statPath(dir + "/primary.bin").inode);
    QCOMPARE(m_fx.readFile(dir + "/victim.bin"), QString(100, QLatin1Char('p')));
    QCOMPARE(m_fx.ls(dir).filter("hlmgr-tmp").size(), 0);
}

HLM_TEST_MAIN(TestHardLinkDialog)

#include "tst_hardlinkdialog.moc"
