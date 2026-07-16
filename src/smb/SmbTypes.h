#pragma once

#include <QDateTime>
#include <QString>

// One entry of an SMB directory listing.
struct FileEntry
{
    QString name;
    bool isDir = false;
    quint64 size = 0;
    QDateTime modified;
    quint64 inode = 0;
    // -1 = unknown: SMB2 directory enumeration never reports link counts, so
    // this stays unset until milestone 3's lazy per-file stat fills it in.
    int nlink = -1;
};
