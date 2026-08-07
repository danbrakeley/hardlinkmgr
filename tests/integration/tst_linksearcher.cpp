#include <QtTest>

#include "core/LinkSearcher.h"
#include "smb/SmbSession.h"

#include "common/SmbFixture.h"
#include "common/TestMain.h"

// The recursive single-root enumeration + inode-grouping pipeline against the
// Samba fixture. The pure grouping math has its own serverless suite
// (tst_linkgrouping); the panel UI on top is covered by tst_linkfinderpanel.
class TestLinkSearcher : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void basicSearch();
    void nonRecursiveStaysShallow();
    void sizeMinExcludesSmallFiles();
    void linkMinFiltersSmallGroups();
    void cancelMidSearch();
    void nonexistentRootFails();
    void unlistableSubfolderCountsAsError();

private:
    struct SearchResult
    {
        QList<LinkSearcher::Entry> entries;
        int folderErrors = 0;
        bool truncated = false;
        bool cancelled = false;
        bool failed = false;
        QString failureMessage;
        int foldersListed = 0; // from the last progress signal
        bool done = false;
    };
    SearchResult search(SmbSession &session, const LinkSearcher::Options &options,
                        int timeoutMs = 30000);

    static QSet<QString> pathSet(const QList<LinkSearcher::Entry> &entries);

    SmbFixture m_fx;
};

void TestLinkSearcher::initTestCase()
{
    SmbSession session;
    HLM_CONNECT_OR_SKIP(m_fx, session);
}

TestLinkSearcher::SearchResult TestLinkSearcher::search(
    SmbSession &session, const LinkSearcher::Options &options, int timeoutMs)
{
    LinkSearcher searcher(&session);
    SearchResult result;
    connect(&searcher, &LinkSearcher::progress, this,
            [&result](int foldersListed, int, int) {
                result.foldersListed = foldersListed;
            });
    connect(&searcher, &LinkSearcher::finished, this,
            [&result](const QList<LinkSearcher::Entry> &entries, int folderErrors,
                      bool truncated, bool cancelled) {
                result.entries = entries;
                result.folderErrors = folderErrors;
                result.truncated = truncated;
                result.cancelled = cancelled;
                result.done = true;
            });
    connect(&searcher, &LinkSearcher::failed, this,
            [&result](const QString &message) {
                result.failed = true;
                result.failureMessage = message;
                result.done = true;
            });

    searcher.start(options);
    [&] { QTRY_VERIFY_WITH_TIMEOUT(result.done, timeoutMs); }();
    return result;
}

QSet<QString> TestLinkSearcher::pathSet(const QList<LinkSearcher::Entry> &entries)
{
    QSet<QString> paths;
    for (const LinkSearcher::Entry &entry : entries) {
        paths.insert(entry.path);
    }
    return paths;
}

void TestLinkSearcher::basicSearch()
{
    const QString dir = m_fx.makeCaseDir("basic");
    m_fx.makeDir(dir + "/sub");
    m_fx.seedFile(dir, "linked.bin", 500);
    m_fx.makeHardLink(dir + "/linked.bin", dir + "/sub/linked2.bin");
    m_fx.seedFile(dir, "alone.bin", 500);

    SmbSession session;
    QVERIFY(m_fx.connectForTest(session));

    LinkSearcher::Options options;
    options.searchPath = dir;
    const SearchResult result = search(session, options);

    QVERIFY(!result.failed);
    QVERIFY(!result.cancelled);
    QCOMPARE(result.folderErrors, 0);
    QCOMPARE(pathSet(result.entries),
             QSet<QString>({dir + "/linked.bin", dir + "/sub/linked2.bin"}));
    for (const LinkSearcher::Entry &entry : result.entries) {
        QCOMPARE(entry.linkCount, 2);
    }
}

void TestLinkSearcher::nonRecursiveStaysShallow()
{
    const QString dir = m_fx.makeCaseDir("nonrecursive");
    m_fx.makeDir(dir + "/sub");
    m_fx.seedFile(dir, "top.bin", 200);
    m_fx.makeHardLink(dir + "/top.bin", dir + "/sub/nested.bin");

    SmbSession session;
    QVERIFY(m_fx.connectForTest(session));

    LinkSearcher::Options options;
    options.searchPath = dir;
    options.recursive = false;
    const SearchResult result = search(session, options);

    // The subfolder was never listed, so the pair's second half was never
    // found — top.bin alone doesn't meet the default Link Min of 2.
    QVERIFY(result.entries.isEmpty());
    QCOMPARE(result.foldersListed, 1);
}

void TestLinkSearcher::sizeMinExcludesSmallFiles()
{
    const QString dir = m_fx.makeCaseDir("sizemin");
    m_fx.seedFile(dir, "big1.bin", 1000);
    m_fx.makeHardLink(dir + "/big1.bin", dir + "/big2.bin");
    m_fx.seedFile(dir, "tiny1.bin", 10);
    m_fx.makeHardLink(dir + "/tiny1.bin", dir + "/tiny2.bin");

    SmbSession session;
    QVERIFY(m_fx.connectForTest(session));

    LinkSearcher::Options options;
    options.searchPath = dir;
    options.sizeMinBytes = 500;
    const SearchResult result = search(session, options);

    QCOMPARE(pathSet(result.entries),
             QSet<QString>({dir + "/big1.bin", dir + "/big2.bin"}));
}

void TestLinkSearcher::linkMinFiltersSmallGroups()
{
    const QString dir = m_fx.makeCaseDir("linkmin");
    m_fx.seedFile(dir, "pair1.bin", 300);
    m_fx.makeHardLink(dir + "/pair1.bin", dir + "/pair2.bin");
    m_fx.seedFile(dir, "trio1.bin", 300);
    m_fx.makeHardLink(dir + "/trio1.bin", dir + "/trio2.bin");
    m_fx.makeHardLink(dir + "/trio1.bin", dir + "/trio3.bin");

    SmbSession session;
    QVERIFY(m_fx.connectForTest(session));

    LinkSearcher::Options options;
    options.searchPath = dir;
    options.linkMinCount = 3;
    const SearchResult result = search(session, options);

    QCOMPARE(pathSet(result.entries),
             QSet<QString>({dir + "/trio1.bin", dir + "/trio2.bin", dir + "/trio3.bin"}));
}

// testing.md-style "Cancel mid-search": finished(cancelled=true) once, and
// late listing replies are dropped silently.
void TestLinkSearcher::cancelMidSearch()
{
    const QString dir = m_fx.makeCaseDir("cancel");
    m_fx.seedManyDirs(dir, 30, 64);

    SmbSession session;
    QVERIFY(m_fx.connectForTest(session));

    LinkSearcher searcher(&session);
    QSignalSpy finishedSpy(&searcher, &LinkSearcher::finished);
    QSignalSpy progressSpy(&searcher, &LinkSearcher::progress);

    LinkSearcher::Options options;
    options.searchPath = dir;
    searcher.start(options);

    QTRY_VERIFY(progressSpy.count() >= 1);
    QVERIFY(searcher.isRunning());
    searcher.cancel();

    QCOMPARE(finishedSpy.count(), 1);
    QVERIFY(finishedSpy.first().at(3).toBool()); // cancelled
    QVERIFY(!searcher.isRunning());

    const int progressSoFar = progressSpy.count();
    QTest::qWait(750);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(progressSpy.count(), progressSoFar);
}

// This doubles as the "path validated at Start Search time" behavior: an
// unlistable root fails the whole search with no separate pre-validation step.
void TestLinkSearcher::nonexistentRootFails()
{
    const QString dir = m_fx.makeCaseDir("badroot");

    SmbSession session;
    QVERIFY(m_fx.connectForTest(session));

    LinkSearcher::Options options;
    options.searchPath = dir + "/missing";
    const SearchResult result = search(session, options);

    QVERIFY(result.failed);
    QVERIFY(result.failureMessage.contains(dir + "/missing"));
}

void TestLinkSearcher::unlistableSubfolderCountsAsError()
{
    const QString dir = m_fx.makeCaseDir("badsub");
    m_fx.makeDir(dir + "/locked");
    m_fx.seedFile(dir, "a.bin", 700);
    m_fx.makeHardLink(dir + "/a.bin", dir + "/b.bin");
    m_fx.seedFile(dir + "/locked", "hidden.bin", 700);
    m_fx.chmodPath(dir + "/locked", "000");

    SmbSession session;
    QVERIFY(m_fx.connectForTest(session));

    LinkSearcher::Options options;
    options.searchPath = dir;
    const SearchResult result = search(session, options);
    m_fx.chmodPath(dir + "/locked", "755");

    QVERIFY(!result.failed);
    QCOMPARE(result.folderErrors, 1);
    // The listable part still produced its group.
    QCOMPARE(pathSet(result.entries), QSet<QString>({dir + "/a.bin", dir + "/b.bin"}));
}

HLM_TEST_MAIN(TestLinkSearcher)

#include "tst_linksearcher.moc"
