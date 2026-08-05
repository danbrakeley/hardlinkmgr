#include "ui/IconUtil.h"

#include <QFileIconProvider>
#include <QFileInfo>
#include <QHash>
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

QIcon osIcon(const QString &fileName, bool isDir)
{
    static QFileIconProvider provider;

    if (isDir) {
        static const QIcon folderIcon = provider.icon(QFileIconProvider::Folder);
        return folderIcon;
    }

    static QHash<QString, QIcon> cache; // extension (lowercased, no dot) -> icon
    const QString extension = QFileInfo(fileName).suffix().toLower();
    auto it = cache.find(extension);
    if (it == cache.end()) {
        it = cache.insert(extension, provider.icon(QFileInfo(fileName)));
    }
    return it.value();
}
