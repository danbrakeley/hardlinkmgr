#include "LinkResultsModel.h"

#include "core/PathUtil.h"

LinkResultsModel::LinkResultsModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int LinkResultsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int LinkResultsModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant LinkResultsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    const linkgrouping::Entry &entry = m_rows.at(index.row());

    switch (index.column()) {
    case LinksColumn:
        if (role == Qt::DisplayRole) {
            return entry.linkCount;
        }
        break;
    case InodeColumn:
        if (role == Qt::DisplayRole) {
            return entry.inode;
        }
        break;
    case NameColumn:
        if (role == Qt::DisplayRole) {
            return pathutil::nameOf(entry.path);
        }
        if (role == Qt::ToolTipRole) {
            return entry.path;
        }
        break;
    case PathColumn:
        if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
            return pathutil::folderOf(entry.path);
        }
        break;
    }
    return {};
}

QVariant LinkResultsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case LinksColumn:
        return tr("Links");
    case InodeColumn:
        return tr("Inode");
    case NameColumn:
        return tr("Name");
    case PathColumn:
        return tr("Path");
    }
    return {};
}

void LinkResultsModel::setResults(const QList<linkgrouping::Entry> &entries)
{
    beginResetModel();
    m_rows = entries;
    endResetModel();
}
