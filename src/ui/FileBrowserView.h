#pragma once

#include <QHash>
#include <QSet>
#include <QWidget>

#include "smb/SmbTypes.h"

class QItemSelectionModel;
class QLabel;
class QLineEdit;
class QModelIndex;
class QToolButton;
class QTreeView;

class FileFilterProxyModel;
class FileListModel;
class SmbSession;

// One filesystem view (README): path box, case-insensitive filter box,
// "matches / total" label, and the file table. The full listing stays in
// memory; the proxy decides what is shown.
class FileBrowserView : public QWidget
{
    Q_OBJECT

public:
    explicit FileBrowserView(SmbSession *session, QWidget *parent = nullptr);

    void navigateTo(const QString &path);

    // For milestone 4's "Link enabled when >=2 files selected" logic.
    QItemSelectionModel *selectionModel() const;

signals:
    void errorOccurred(const QString &message); // surfaced in the main status bar

private:
    void onPathEdited();
    void onEntryActivated(const QModelIndex &proxyIndex);
    void onDirectoryListed(const QString &path, const QList<FileEntry> &entries);
    void onDirectoryListFailed(const QString &path, const QString &message);
    void onFileStatted(const QString &path, int nlink, quint64 inode);
    void onStatFailed(const QString &path, const QString &message);
    void updateCountLabel();

    QString entryPath(const QString &name) const;
    void resetStatQueue();
    void pumpStats();
    int nextStatRow();

    static QString normalizePath(const QString &path);

    SmbSession *m_session = nullptr;
    FileListModel *m_model = nullptr;
    FileFilterProxyModel *m_proxy = nullptr;
    QTreeView *m_tree = nullptr;
    QToolButton *m_upButton = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QToolButton *m_foldersFirstButton = nullptr;
    QToolButton *m_caseSensitiveButton = nullptr;
    QLineEdit *m_filterEdit = nullptr;
    QLabel *m_countLabel = nullptr;
    QString m_currentPath;  // last successfully listed path
    QString m_pendingPath;  // path of the in-flight listing, empty if none

    // Lazy link-count fill-in (milestone 3): a bounded number of stats is in
    // flight, visible rows are served first, and navigation resets everything
    // (replies for the old directory miss m_statInFlight and are dropped).
    QList<int> m_statOrder;          // source rows of all files, listing order
    int m_statCursor = 0;            // next sequential candidate in m_statOrder
    QSet<int> m_statRequested;       // source rows already sent (or done)
    QHash<QString, int> m_statInFlight; // full path -> source row
};
