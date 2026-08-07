#include <QtTest>

#include "core/LinkGrouping.h"

#include "common/TestMain.h"

using linkgrouping::FileRecord;
using linkgrouping::Result;

namespace {

QSet<QString> pathSet(const QList<linkgrouping::Entry> &entries)
{
    QSet<QString> paths;
    for (const linkgrouping::Entry &entry : entries) {
        paths.insert(entry.path);
    }
    return paths;
}

} // namespace

class TestLinkGrouping : public QObject
{
    Q_OBJECT

private slots:
    void basicGrouping();
    void zeroInodeNeverGrouped();
    void linkMinFiltersSmallBuckets();
    void linkMinZeroAdmitsEveryFile();
    void linkCountReflectsBucketSize();
    void truncatesAtMaxEntries();
};

// Two files share an inode; a third, unrelated file has a unique inode and
// (at the default Link Min of 2) is excluded.
void TestLinkGrouping::basicGrouping()
{
    const Result r = linkgrouping::computeGroups(
        {
            {"/a/one.bin", 100, 10},
            {"/a/two.bin", 100, 10},
            {"/a/three.bin", 100, 11},
        },
        /*linkMinCount*/ 2);
    QVERIFY(!r.truncated);
    QCOMPARE(pathSet(r.entries), QSet<QString>({"/a/one.bin", "/a/two.bin"}));
}

// inode == 0 means "unknown" — never treated as a shared group, no matter how
// many files report it.
void TestLinkGrouping::zeroInodeNeverGrouped()
{
    const Result r = linkgrouping::computeGroups(
        {{"/a/one.bin", 100, 0}, {"/a/two.bin", 100, 0}}, 2);
    QCOMPARE(r.entries.size(), 0);
}

void TestLinkGrouping::linkMinFiltersSmallBuckets()
{
    const QList<FileRecord> files = {
        {"/a/one.bin", 100, 10},
        {"/a/two.bin", 100, 10},
        {"/a/three.bin", 100, 11},
        {"/a/four.bin", 100, 11},
        {"/a/five.bin", 100, 11},
    };
    QCOMPARE(pathSet(linkgrouping::computeGroups(files, 3).entries),
             QSet<QString>({"/a/three.bin", "/a/four.bin", "/a/five.bin"}));
    QCOMPARE(pathSet(linkgrouping::computeGroups(files, 4).entries), QSet<QString>());
}

// Link Min of 0 or 1 both mean "no additional filter" — every file with a
// valid inode qualifies, same as a bucket of size 1.
void TestLinkGrouping::linkMinZeroAdmitsEveryFile()
{
    const Result r = linkgrouping::computeGroups({{"/a/lonely.bin", 100, 42}}, 0);
    QCOMPARE(r.entries.size(), 1);
}

void TestLinkGrouping::linkCountReflectsBucketSize()
{
    const Result r = linkgrouping::computeGroups(
        {
            {"/a/one.bin", 100, 10},
            {"/a/two.bin", 100, 10},
            {"/a/three.bin", 100, 10},
        },
        2);
    QCOMPARE(r.entries.size(), 3);
    for (const linkgrouping::Entry &entry : r.entries) {
        QCOMPARE(entry.linkCount, 3);
        QCOMPARE(entry.inode, 10ULL);
    }
}

void TestLinkGrouping::truncatesAtMaxEntries()
{
    const Result r = linkgrouping::computeGroups(
        {
            {"/a/one.bin", 100, 10},
            {"/a/two.bin", 100, 10},
            {"/a/three.bin", 100, 10},
        },
        2, /*maxEntries*/ 2);
    QVERIFY(r.truncated);
    QCOMPARE(r.entries.size(), 2);
}

HLM_TEST_MAIN(TestLinkGrouping)

#include "tst_linkgrouping.moc"
