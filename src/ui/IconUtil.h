#pragma once

#include <QColor>
#include <QIcon>
#include <QSize>
#include <QString>

// Loads an SVG resource and paints it flat in `color`: the SVG only supplies
// shape (alpha), not color, so a single icon asset can follow the palette
// instead of a baked-in fill.
QIcon coloredIcon(const QString &resourcePath, const QColor &color, QSize size, qreal devicePixelRatio);
