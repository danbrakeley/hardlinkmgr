#pragma once

#include <QHash>
#include <QList>
#include <QString>

// The pure grouping math behind LinkSearcher, kept free of the traversal so
// the bucketing/filtering edge cases are testable without a server.
// LinkSearcher gathers FileRecords from directory enumerations and hands them
// here. Bucketing by inode is purely a computation (each file's Links count,
// and the Link Min filter) — it imposes no display order; the results table
// itself is sortable by any column.
namespace linkgrouping {

struct FileRecord
{
    QString path;
    quint64 size = 0;
    quint64 inode = 0;
};

// One row of the Link Finder results table.
struct Entry
{
    QString path;
    quint64 size = 0;
    quint64 inode = 0;
    int linkCount = 0; // files sharing this inode found under the search root
};

struct Result
{
    QList<Entry> entries;
    bool truncated = false;
};

constexpr int kMaxEntries = 100000;

// Buckets files by inode (files with inode == 0 — never returned by a real
// server — are dropped, since there's nothing to group them by), keeps
// buckets whose size is >= linkMinCount, and flattens each surviving bucket
// into one Entry per file. sizeMinBytes filtering happens during traversal
// (LinkSearcher only ever gathers qualifying files), not here.
inline Result computeGroups(const QList<FileRecord> &files, int linkMinCount,
                            int maxEntries = kMaxEntries)
{
    QHash<quint64, QList<const FileRecord *>> buckets;
    for (const FileRecord &file : files) {
        if (file.inode == 0) {
            continue;
        }
        buckets[file.inode].append(&file);
    }

    Result result;
    for (auto it = buckets.constBegin(); it != buckets.constEnd(); ++it) {
        const QList<const FileRecord *> &bucket = it.value();
        if (bucket.size() < linkMinCount) {
            continue;
        }
        for (const FileRecord *file : bucket) {
            if (result.entries.size() >= maxEntries) {
                result.truncated = true;
                return result;
            }
            result.entries.append({file->path, file->size, file->inode, int(bucket.size())});
        }
    }
    return result;
}

} // namespace linkgrouping
