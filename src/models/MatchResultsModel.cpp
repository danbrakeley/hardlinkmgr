#include "MatchResultsModel.h"

#include "core/PathUtil.h"

MatchResultsModel::MatchResultsModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int MatchResultsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int MatchResultsModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant MatchResultsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    const Row &row = m_rows.at(index.row());

    switch (index.column()) {
    case CheckColumn:
        if (role == Qt::CheckStateRole) {
            return row.checked ? Qt::Checked : Qt::Unchecked;
        }
        break;
    case PrimaryColumn:
        if (role == Qt::DisplayRole) {
            return pathutil::nameOf(row.match.primaryPath);
        }
        if (role == Qt::ToolTipRole) {
            return row.match.primaryPath;
        }
        break;
    case SecondaryColumn:
        if (role == Qt::DisplayRole) {
            return pathutil::nameOf(row.match.secondaryPath);
        }
        if (role == Qt::ToolTipRole) {
            return row.match.secondaryPath;
        }
        break;
    case StatusColumn:
        if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
            return row.status;
        }
        break;
    }
    return {};
}

QVariant MatchResultsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case CheckColumn:
        return {}; // the header checkbox is painted by CheckBoxHeader
    case PrimaryColumn:
        return tr("Primary Name");
    case SecondaryColumn:
        return tr("Secondary Name");
    case StatusColumn:
        return tr("Status");
    }
    return {};
}

Qt::ItemFlags MatchResultsModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == CheckColumn && !m_locked) {
        f |= Qt::ItemIsUserCheckable;
    }
    return f;
}

bool MatchResultsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.column() != CheckColumn
        || role != Qt::CheckStateRole || m_locked) {
        return false;
    }
    Row &row = m_rows[index.row()];
    const bool checked = static_cast<Qt::CheckState>(value.toInt()) == Qt::Checked;
    if (row.checked == checked) {
        return true;
    }
    row.checked = checked;
    m_checkedCount += checked ? 1 : -1;
    emit dataChanged(index, index, {Qt::CheckStateRole});
    emit checkedCountChanged(m_checkedCount);
    return true;
}

void MatchResultsModel::setMatches(const QList<MatchSearcher::Match> &matches)
{
    beginResetModel();
    m_rows.clear();
    m_rows.reserve(matches.size());
    for (const MatchSearcher::Match &match : matches) {
        m_rows.append({match, false, QString()});
    }
    m_checkedCount = 0;
    endResetModel();
    emit checkedCountChanged(m_checkedCount);
}

QList<int> MatchResultsModel::checkedRows() const
{
    QList<int> rows;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).checked) {
            rows.append(i);
        }
    }
    return rows;
}

void MatchResultsModel::setAllChecked(bool checked)
{
    if (m_locked || m_rows.isEmpty()) {
        return;
    }
    bool changed = false;
    for (Row &row : m_rows) {
        if (row.checked != checked) {
            row.checked = checked;
            changed = true;
        }
    }
    if (!changed) {
        return;
    }
    m_checkedCount = checked ? m_rows.size() : 0;
    emit dataChanged(index(0, CheckColumn), index(m_rows.size() - 1, CheckColumn),
                     {Qt::CheckStateRole});
    emit checkedCountChanged(m_checkedCount);
}

Qt::CheckState MatchResultsModel::aggregateCheckState() const
{
    if (m_rows.isEmpty() || m_checkedCount == 0) {
        return Qt::Unchecked;
    }
    return m_checkedCount == m_rows.size() ? Qt::Checked : Qt::PartiallyChecked;
}

void MatchResultsModel::setStatus(int row, const QString &text)
{
    m_rows[row].status = text;
    const QModelIndex idx = index(row, StatusColumn);
    emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::ToolTipRole});
}
