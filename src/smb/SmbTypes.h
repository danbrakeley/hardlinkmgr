#pragma once

#include <QDateTime>
#include <QString>

// One entry of an SMB directory listing.
struct FileEntry
{
    // SMB2 directory enumeration never reports link counts; entries start
    // Unknown and the lazy per-file stat fills them in (Unavailable if it
    // fails, e.g. no permission).
    static constexpr int kNlinkUnknown = -1;
    static constexpr int kNlinkUnavailable = -2;

    QString name;
    bool isDir = false;
    quint64 size = 0;
    QDateTime modified;
    quint64 inode = 0;
    int nlink = kNlinkUnknown;
};
