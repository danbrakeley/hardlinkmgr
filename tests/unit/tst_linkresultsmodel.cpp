#include <QtTest>

#include "models/LinkResultsModel.h"

#include "common/TestMain.h"

namespace {

QList<linkgrouping::Entry> sampleEntries()
{
    return {
        {"/media/a/one.iso", 100, 10, 2},
        {"/media/a/two.iso", 100, 10, 2},
        {"/media/b/popular.iso", 200, 11, 4},
    };
}

} // namespace

class TestLinkResultsModel : public QObject
{
    Q_OBJECT

private slots:
    void populates();
    void columnsShowNameAndFolder();
    void setResultsReplacesRows();
};

void TestLinkResultsModel::populates()
{
    LinkResultsModel model;
    model.setResults(sampleEntries());

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.columnCount(), int(LinkResultsModel::ColumnCount));
    QCOMPARE(model.index(0, LinkResultsModel::LinksColumn).data().toInt(), 2);
    QCOMPARE(model.index(0, LinkResultsModel::InodeColumn).data().toULongLong(), 10ULL);
    QCOMPARE(model.entryAt(2).path, "/media/b/popular.iso");
}

// Name shows the bare filename; Path shows the containing folder (matching
// the roadmap diagram's example rows), with the full path in the tooltip.
void TestLinkResultsModel::columnsShowNameAndFolder()
{
    LinkResultsModel model;
    model.setResults(sampleEntries());

    QCOMPARE(model.index(0, LinkResultsModel::NameColumn).data().toString(), "one.iso");
    QCOMPARE(model.index(0, LinkResultsModel::NameColumn).data(Qt::ToolTipRole).toString(),
             "/media/a/one.iso");
    QCOMPARE(model.index(0, LinkResultsModel::PathColumn).data().toString(), "/media/a");
    QCOMPARE(model.index(2, LinkResultsModel::PathColumn).data().toString(), "/media/b");
}

void TestLinkResultsModel::setResultsReplacesRows()
{
    LinkResultsModel model;
    model.setResults(sampleEntries());
    QCOMPARE(model.rowCount(), 3);

    model.setResults({});
    QCOMPARE(model.rowCount(), 0);
}

HLM_TEST_MAIN(TestLinkResultsModel)

#include "tst_linkresultsmodel.moc"
