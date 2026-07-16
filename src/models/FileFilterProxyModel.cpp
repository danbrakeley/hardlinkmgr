#include "FileFilterProxyModel.h"

#include "FileListModel.h"

FileFilterProxyModel::FileFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSortRole(FileListModel::SortRole);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setFilterKeyColumn(FileListModel::NameColumn);
}

// lessThan() has no access to the active sort order, but folders must stay on
// top in both directions — so remember the order and pre-invert below.
void FileFilterProxyModel::sort(int column, Qt::SortOrder order)
{
    m_order = order;
    QSortFilterProxyModel::sort(column, order);
}

bool FileFilterProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const bool leftIsDir = left.data(FileListModel::IsDirRole).toBool();
    const bool rightIsDir = right.data(FileListModel::IsDirRole).toBool();
    if (leftIsDir != rightIsDir) {
        return m_order == Qt::AscendingOrder ? leftIsDir : rightIsDir;
    }
    return QSortFilterProxyModel::lessThan(left, right);
}
