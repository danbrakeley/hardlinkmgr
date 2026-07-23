#pragma once

#include <QDir>
#include <QString>

// Helpers for share-absolute paths ("/" is the share root).
namespace pathutil {

// Cleans up hand-typed paths: guarantees a leading '/', collapses "//", "."
// and "..", drops trailing slashes.
inline QString normalize(const QString &path)
{
    QString p = path.trimmed();
    if (!p.startsWith(QLatin1Char('/'))) {
        p.prepend(QLatin1Char('/'));
    }
    p = QDir::cleanPath(p);
    return p.isEmpty() ? QStringLiteral("/") : p;
}

inline QString join(const QString &folder, const QString &name)
{
    return folder == QLatin1String("/")
               ? QLatin1Char('/') + name
               : folder + QLatin1Char('/') + name;
}

inline QString folderOf(const QString &path)
{
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    return slash <= 0 ? QStringLiteral("/") : path.left(slash);
}

inline QString nameOf(const QString &path)
{
    return path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
}

} // namespace pathutil
