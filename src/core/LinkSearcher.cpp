#include "LinkSearcher.h"

#include "core/MatchPairing.h" // isTmpName
#include "core/PathUtil.h"
#include "smb/SmbSession.h"

namespace {

// Same bound as MatchSearcher — enumerations are heavier than stats, so this
// sits well below FileBrowserView's stat bound; enough to keep the pipe full
// on a LAN.
constexpr int kMaxListsInFlight = 8;

} // namespace

LinkSearcher::LinkSearcher(SmbSession *session, QObject *parent)
    : QObject(parent)
    , m_session(session)
{
    connect(m_session, &SmbSession::directoryListed,
            this, &LinkSearcher::onDirectoryListed);
    connect(m_session, &SmbSession::directoryListFailed,
            this, &LinkSearcher::onDirectoryListFailed);
}

void LinkSearcher::start(const Options &options)
{
    if (m_running) {
        return;
    }
    m_options = options;
    resetState();
    m_running = true;
    enqueueDir(m_options.searchPath);
    pumpListings();
}

void LinkSearcher::cancel()
{
    if (!m_running) {
        return;
    }
    const int folderErrors = m_folderErrors;
    m_running = false;
    resetState(); // late replies now miss m_inFlight and are dropped
    emit finished({}, folderErrors, false, /*cancelled*/ true);
}

void LinkSearcher::enqueueDir(const QString &path)
{
    if (!m_visited.contains(path)) {
        m_visited.insert(path);
        m_queue.append(path);
    }
}

void LinkSearcher::pumpListings()
{
    if (m_inPump) {
        return; // a synchronous failure re-entered us; the outer loop continues
    }
    m_inPump = true;
    while (m_running && m_inFlight.size() < kMaxListsInFlight && !m_queue.isEmpty()) {
        const QString path = m_queue.takeFirst();
        m_inFlight.insert(path);
        m_session->listDirectory(path); // may fail synchronously
    }
    m_inPump = false;
    maybeFinish();
}

void LinkSearcher::maybeFinish()
{
    if (!m_running || m_inPump || !m_queue.isEmpty() || !m_inFlight.isEmpty()) {
        return;
    }
    m_running = false;
    computeResults();
}

void LinkSearcher::onDirectoryListed(const QString &path, const QList<FileEntry> &entries)
{
    if (!m_running || !m_inFlight.remove(path)) {
        return; // some other consumer's listing, or ours after a cancel
    }
    ++m_foldersListed;

    for (const FileEntry &entry : entries) {
        const QString entryPath = pathutil::join(path, entry.name);
        if (entry.isDir) {
            if (m_options.recursive) {
                enqueueDir(entryPath);
            }
        } else if (entry.size >= m_options.sizeMinBytes
                   && !matchpairing::isTmpName(entry.name)) {
            m_files.append({entryPath, entry.size, entry.inode});
        }
    }

    emit progress(m_foldersListed, m_queue.size() + m_inFlight.size(), m_files.size());
    pumpListings();
}

void LinkSearcher::onDirectoryListFailed(const QString &path, const QString &message)
{
    if (!m_running || !m_inFlight.remove(path)) {
        return;
    }
    if (path == m_options.searchPath) {
        // The root can't be listed: the search as a whole is invalid. This is
        // also how a bad path gets rejected at Start-Search time.
        m_running = false;
        resetState();
        emit failed(tr("Could not list %1: %2").arg(path, message));
        return;
    }
    // One unlistable subfolder (permissions, ...) shouldn't kill the search.
    ++m_folderErrors;
    emit progress(m_foldersListed, m_queue.size() + m_inFlight.size(), m_files.size());
    pumpListings();
}

// The grouping itself is pure and lives in core/LinkGrouping.h.
void LinkSearcher::computeResults()
{
    // Take everything the traversal gathered so the searcher is idle (and
    // restartable, even from a finished handler) before the signal goes out.
    QList<linkgrouping::FileRecord> files = std::move(m_files);
    const int folderErrors = m_folderErrors;
    resetState();

    const linkgrouping::Result result =
        linkgrouping::computeGroups(files, m_options.linkMinCount);
    emit finished(result.entries, folderErrors, result.truncated, /*cancelled*/ false);
}

void LinkSearcher::resetState()
{
    m_visited.clear();
    m_queue.clear();
    m_inFlight.clear();
    m_files.clear();
    m_foldersListed = 0;
    m_folderErrors = 0;
}
