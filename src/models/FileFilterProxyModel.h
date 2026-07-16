#pragma once

#include <QSortFilterProxyModel>

// Sort/filter layer over FileListModel: folders always group above files no
// matter which column or direction is sorted; within each group the usual
// column comparison applies (case-insensitive for names). Filtering is the
// inherited fixed-string "contains" match, case-insensitive, on the name.
class FileFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit FileFilterProxyModel(QObject *parent = nullptr);

    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    Qt::SortOrder m_order = Qt::AscendingOrder;
};
