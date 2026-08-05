#pragma once

#include <QColor>
#include <QIcon>
#include <QSize>
#include <QString>

// Loads an SVG resource and paints it flat in `color`: the SVG only supplies
// shape (alpha), not color, so a single icon asset can follow the palette
// instead of a baked-in fill.
QIcon coloredIcon(const QString &resourcePath, const QColor &color, QSize size, qreal devicePixelRatio);

// Host-OS icon for a (possibly remote/nonexistent) file, looked up by
// extension only — no local file access is required or attempted. Results
// are cached per extension (and once for folders), since OS icon lookups
// aren't free and this is called once per visible row.
QIcon osIcon(const QString &fileName, bool isDir);
