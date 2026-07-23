#include "CheckBoxHeader.h"

#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionButton>

CheckBoxHeader::CheckBoxHeader(Qt::Orientation orientation, QWidget *parent)
    : QHeaderView(orientation, parent)
{
}

void CheckBoxHeader::setCheckState(Qt::CheckState state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    updateSection(0);
}

void CheckBoxHeader::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    painter->save();
    QHeaderView::paintSection(painter, rect, logicalIndex);
    painter->restore();

    if (logicalIndex != 0) {
        return;
    }

    QStyleOptionButton option;
    option.state = QStyle::State_Enabled | QStyle::State_Active;
    switch (m_state) {
    case Qt::Checked:
        option.state |= QStyle::State_On;
        break;
    case Qt::PartiallyChecked:
        option.state |= QStyle::State_NoChange;
        break;
    case Qt::Unchecked:
        option.state |= QStyle::State_Off;
        break;
    }
    const int w = style()->pixelMetric(QStyle::PM_IndicatorWidth, &option, this);
    const int h = style()->pixelMetric(QStyle::PM_IndicatorHeight, &option, this);
    option.rect = QRect(rect.center().x() - w / 2, rect.center().y() - h / 2, w, h);
    style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &option, painter, this);
}

void CheckBoxHeader::mousePressEvent(QMouseEvent *event)
{
    if (logicalIndexAt(event->pos()) == 0) {
        emit toggleAllRequested(m_state != Qt::Checked);
        return;
    }
    QHeaderView::mousePressEvent(event);
}
