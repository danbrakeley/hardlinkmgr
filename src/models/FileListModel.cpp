#include "FileListModel.h"

#include <QApplication>
#include <QLocale>
#include <QStyle>

FileListModel::FileListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    QStyle *style = QApplication::style();
    m_dirIcon = style->standardIcon(QStyle::SP_DirIcon);
    m_fileIcon = style->standardIcon(QStyle::SP_FileIcon);
}

void FileListModel::setEntries(const QList<FileEntry> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

int FileListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

int FileListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant FileListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }
    const FileEntry &entry = m_entries.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case NameColumn:
            return entry.name;
        case SizeColumn:
            return entry.isDir ? QVariant()
                               : QVariant(QLocale().formattedDataSize(qint64(entry.size)));
        case ModifiedColumn:
            return QLocale().toString(entry.modified, QLocale::ShortFormat);
        case LinksColumn:
            if (entry.isDir) {
                return {};
            }
            return entry.nlink < 0 ? QStringLiteral("…") : QString::number(entry.nlink);
        case InodeColumn:
            return QString::number(entry.inode);
        }
        break;

    case Qt::DecorationRole:
        if (index.column() == IconColumn) {
            return entry.isDir ? m_dirIcon : m_fileIcon;
        }
        break;

    case Qt::TextAlignmentRole:
        switch (index.column()) {
        case SizeColumn:
        case LinksColumn:
        case InodeColumn:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        }
        break;

    case SortRole:
        switch (index.column()) {
        case IconColumn: // sorting the icon column behaves like sorting names
        case NameColumn:
            return entry.name;
        case SizeColumn:
            return QVariant::fromValue<qulonglong>(entry.size);
        case ModifiedColumn:
            return entry.modified;
        case LinksColumn:
            return entry.nlink;
        case InodeColumn:
            return QVariant::fromValue<qulonglong>(entry.inode);
        }
        break;

    case IsDirRole:
        return entry.isDir;
    }

    return {};
}

QVariant FileListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case IconColumn:     return QString();
    case NameColumn:     return tr("Name");
    case SizeColumn:     return tr("Size");
    case ModifiedColumn: return tr("Modified");
    case LinksColumn:    return tr("Links");
    case InodeColumn:    return tr("Inode");
    }
    return {};
}
