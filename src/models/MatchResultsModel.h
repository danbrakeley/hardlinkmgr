#pragma once

#include <QAbstractTableModel>
#include <QList>

#include "core/MatchSearcher.h"

// The Match Finder results: one row per candidate pair, with a user checkbox
// (which pairs to link) and a status column that fills in during a link run.
class MatchResultsModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        CheckColumn = 0,
        PrimaryColumn,
        SecondaryColumn,
        StatusColumn,
        ColumnCount,
    };

    explicit MatchResultsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    void setMatches(const QList<MatchSearcher::Match> &matches); // clears checks/statuses
    const MatchSearcher::Match &matchAt(int row) const { return m_rows.at(row).match; }

    int checkedCount() const { return m_checkedCount; }
    QList<int> checkedRows() const;
    void setAllChecked(bool checked);
    Qt::CheckState aggregateCheckState() const; // tri-state, for the header checkbox

    void setStatus(int row, const QString &text);

    // While locked (a link run is in progress) the checkboxes are read-only.
    void setLocked(bool locked) { m_locked = locked; }

signals:
    void checkedCountChanged(int count);

private:
    struct Row
    {
        MatchSearcher::Match match;
        bool checked = false;
        QString status;
    };

    QList<Row> m_rows;
    int m_checkedCount = 0;
    bool m_locked = false;
};
