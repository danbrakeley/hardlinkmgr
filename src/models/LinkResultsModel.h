#pragma once

#include <QAbstractTableModel>
#include <QList>

#include "core/LinkGrouping.h"

// The Link Finder results: one row per file that belongs to a hard-link
// group at or above Link Min, sortable by any column (via the proxy model
// LinkFinderPanel wraps this in — inode-bucketing is a computation, not a
// display order, so this model imposes none of its own).
class LinkResultsModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        LinksColumn = 0,
        InodeColumn,
        NameColumn,
        PathColumn,
        ColumnCount,
    };

    explicit LinkResultsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void setResults(const QList<linkgrouping::Entry> &entries);
    const linkgrouping::Entry &entryAt(int row) const { return m_rows.at(row); }

private:
    QList<linkgrouping::Entry> m_rows;
};
