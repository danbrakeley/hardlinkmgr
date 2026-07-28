#pragma once

#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

#include "core/MatchPairing.h"
#include "smb/SmbTypes.h"

class SmbSession;

// Recursively enumerates the primary and secondary folders and pairs up files
// whose sizes are within the given difference — candidate near-duplicates for
// hard-linking. Works entirely from directory-enumeration data (name, size,
// inode); pairs that already share an inode are dropped as already linked.
//
// The two folder trees may overlap or be identical: every directory is listed
// at most once, and its primary/secondary membership is derived from its path
// relative to the two roots.
//
// Listings run on the shared SmbSession with bounded concurrency. Replies are
// correlated by path, so a cancelled search simply stops scheduling and drops
// late replies (SmbSession has no per-operation cancel).
class MatchSearcher : public QObject
{
    Q_OBJECT

public:
    struct Options
    {
        QString primaryPath;   // normalized, share-absolute
        QString secondaryPath; // normalized, share-absolute
        bool primaryRecursive = true;
        bool secondaryRecursive = true;
        quint64 sizeMinBytes = 0;  // 0 = no minimum
        quint64 sizeDiffBytes = 0; // always applied; 0 = exact size only
    };

    // The pairing math (and this struct) live in core/MatchPairing.h; the
    // alias keeps MatchSearcher::Match the name the rest of the app uses.
    using Match = matchpairing::Match;

    explicit MatchSearcher(SmbSession *session, QObject *parent = nullptr);

    bool isRunning() const { return m_running; }

    void start(const Options &options); // no-op while running
    void cancel();                      // emits finished(..., cancelled=true)

signals:
    void progress(int foldersListed, int foldersPending, int filesGathered);
    void finished(const QList<MatchSearcher::Match> &matches,
                  int folderErrors, bool truncated, bool cancelled);
    void failed(const QString &message); // a root path could not be listed

private:
    quint8 sideMaskOf(const QString &dirPath) const;
    void enqueueDir(const QString &path);
    void pumpListings();
    void maybeFinish();
    void computeMatches();
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
    QList<matchpairing::FileRecord> m_files;
    int m_foldersListed = 0;
    int m_folderErrors = 0;
};
