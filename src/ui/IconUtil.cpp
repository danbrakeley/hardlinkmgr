#include "ui/IconUtil.h"

#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSvgRenderer>

QIcon coloredIcon(const QString &resourcePath, const QColor &color, QSize size, qreal devicePixelRatio)
{
    QPixmap pixmap(size * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    QSvgRenderer renderer(resourcePath);
    renderer.render(&painter, QRectF(QPointF(0, 0), size));

    // Keeps the shape's alpha but replaces every painted pixel with `color`.
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(QRect(QPoint(0, 0), size), color);

    return QIcon(pixmap);
}
