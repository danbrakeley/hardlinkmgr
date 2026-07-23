#pragma once

#include <QHeaderView>

// A horizontal header that paints a tri-state checkbox in section 0; clicking
// that section requests checking/unchecking every row. Qt has no built-in
// header checkbox, so this is the stock recipe: paintSection draws the
// indicator, mousePressEvent turns the click into a signal, and the owner
// pushes the aggregate state back in via setCheckState.
class CheckBoxHeader : public QHeaderView
{
    Q_OBJECT

public:
    explicit CheckBoxHeader(Qt::Orientation orientation, QWidget *parent = nullptr);

    void setCheckState(Qt::CheckState state);

signals:
    void toggleAllRequested(bool checkAll);

protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    Qt::CheckState m_state = Qt::Unchecked;
};
