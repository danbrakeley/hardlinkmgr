#pragma once

#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

#include "core/LinkGrouping.h"
#include "smb/SmbTypes.h"

class SmbSession;

// Recursively enumerates a single folder and groups files that already share
// an inode — existing hard-link groups the user can browse straight to.
// Works entirely from directory-enumeration data (name, size, inode); the
// grouping math itself lives in core/LinkGrouping.h.
//
// Listings run on the shared SmbSession with bounded concurrency. Replies are
// correlated by path, so a cancelled search simply stops scheduling and drops
// late replies (SmbSession has no per-operation cancel). If the search root
// itself can't be listed, failed() fires — this doubles as the "path is
// validated when Start Search is clicked" behavior, with no separate
// pre-validation step needed.
class LinkSearcher : public QObject
{
    Q_OBJECT

public:
    struct Options
    {
        QString searchPath; // normalized, share-absolute
        bool recursive = true;
        quint64 sizeMinBytes = 0; // 0 = no minimum
        int linkMinCount = 2;
    };

    using Entry = linkgrouping::Entry;

    explicit LinkSearcher(SmbSession *session, QObject *parent = nullptr);

    bool isRunning() const { return m_running; }

    void start(const Options &options); // no-op while running
    void cancel();                      // emits finished(..., cancelled=true)

signals:
    void progress(int foldersListed, int foldersPending, int filesGathered);
    void finished(const QList<LinkSearcher::Entry> &entries,
                  int folderErrors, bool truncated, bool cancelled);
    void failed(const QString &message); // the search root could not be listed

private:
    void enqueueDir(const QString &path);
    void pumpListings();
    void maybeFinish();
    void computeResults();
    void resetState();
    void onDirectoryListed(const QString &path, const QList<FileEntry> &entries);
    void onDirectoryListFailed(const QString &path, const QString &message);

    SmbSession *m_session = nullptr;
    Options m_options;
    bool m_running = false;
    bool m_inPump = false;      // pumpListings can be re-entered by a
                                // synchronously-failing listDirectory
    QSet<QString> m_visited;    // dirs scheduled (queued, in flight, or done)
    QList<QString> m_queue;     // dirs waiting for a free listing slot
    QSet<QString> m_inFlight;   // dirs with a listing outstanding
    QList<linkgrouping::FileRecord> m_files;
    int m_foldersListed = 0;
    int m_folderErrors = 0;
};
