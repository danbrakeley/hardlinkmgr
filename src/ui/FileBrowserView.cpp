#include "FileBrowserView.h"

#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QScrollBar>
#include <QStyle>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace {

// Upper bound on stats in flight per view. High enough to keep the pipe full
// on a LAN, low enough that navigation away leaves little wasted work (the
// unsent backlog is simply dropped).
constexpr int kMaxStatsInFlight = 32;

// There is no stock "case sensitivity" icon, so paint an "Aa".
QIcon makeCaseSensitivityIcon(QFont font, const QPalette &palette)
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::TextAntialiasing);
    font.setBold(true);
    font.setPixelSize(20);
    painter.setFont(font);
    painter.setPen(palette.color(QPalette::ButtonText));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("Aa"));
    return QIcon(pixmap);
}

} // namespace

#include "models/FileFilterProxyModel.h"
#include "models/FileListModel.h"
#include "smb/SmbSession.h"

FileBrowserView::FileBrowserView(SmbSession *session, QWidget *parent)
    : QWidget(parent)
    , m_session(session)
{
    m_model = new FileListModel(this);
    m_proxy = new FileFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);

    m_upButton = new QToolButton(this);
    m_upButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogToParent));
    m_upButton->setToolTip(tr("Go up to the parent folder"));
    m_upButton->setAutoRaise(true);
    m_upButton->setEnabled(false); // no parent until a listing below "/" succeeds
    connect(m_upButton, &QToolButton::clicked, this, [this] {
        if (m_currentPath.isEmpty() || m_currentPath == QLatin1String("/")) {
            return;
        }
        const int slash = m_currentPath.lastIndexOf(QLatin1Char('/'));
        navigateTo(slash <= 0 ? QStringLiteral("/") : m_currentPath.left(slash));
    });

    m_pathEdit = new QLineEdit(QStringLiteral("/"), this);
    connect(m_pathEdit, &QLineEdit::returnPressed,
            this, &FileBrowserView::onPathEdited);

    m_foldersFirstButton = new QToolButton(this);
    m_foldersFirstButton->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
    m_foldersFirstButton->setCheckable(true);
    m_foldersFirstButton->setChecked(true);
    m_foldersFirstButton->setAutoRaise(true);
    m_foldersFirstButton->setToolTip(tr("Keep folders sorted to the top.\n"
                                        "When off, folders sort among the files."));
    connect(m_foldersFirstButton, &QToolButton::toggled, this, [this](bool on) {
        m_proxy->setFoldersFirst(on);
    });

    m_caseSensitiveButton = new QToolButton(this);
    m_caseSensitiveButton->setIcon(makeCaseSensitivityIcon(font(), palette()));
    m_caseSensitiveButton->setCheckable(true); // off by default: case-insensitive
    m_caseSensitiveButton->setAutoRaise(true);
    m_caseSensitiveButton->setToolTip(tr("Sort names case-sensitively (uppercase sorts before lowercase).\n"
                                         "When off, sorting ignores case."));
    connect(m_caseSensitiveButton, &QToolButton::toggled, this, [this](bool on) {
        m_proxy->setSortCaseSensitivity(on ? Qt::CaseSensitive : Qt::CaseInsensitive);
    });

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter"));
    m_filterEdit->setClearButtonEnabled(true);
    connect(m_filterEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_proxy->setFilterFixedString(text);
        updateCountLabel();
    });

    m_countLabel = new QLabel(this);

    m_tree = new QTreeView(this);
    m_tree->setModel(m_proxy);
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setAllColumnsShowFocus(true);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(FileListModel::NameColumn, Qt::AscendingOrder);
    connect(m_tree, &QTreeView::activated,
            this, &FileBrowserView::onEntryActivated);
    connect(m_tree->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this] { emit selectionChanged(); });

    QHeaderView *header = m_tree->header();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(FileListModel::NameColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(FileListModel::IconColumn, QHeaderView::Fixed);
    header->resizeSection(FileListModel::IconColumn,
                          m_tree->iconSize().isValid() ? m_tree->iconSize().width() + 12 : 28);

    auto *toolbarLayout = new QHBoxLayout;
    toolbarLayout->addWidget(m_upButton);
    toolbarLayout->addWidget(m_pathEdit, /*stretch*/ 3);
    toolbarLayout->addWidget(m_foldersFirstButton);
    toolbarLayout->addWidget(m_caseSensitiveButton);
    toolbarLayout->addWidget(m_filterEdit, /*stretch*/ 1);
    toolbarLayout->addWidget(m_countLabel);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addLayout(toolbarLayout);
    layout->addWidget(m_tree);

    connect(m_session, &SmbSession::directoryListed,
            this, &FileBrowserView::onDirectoryListed);
    connect(m_session, &SmbSession::directoryListFailed,
            this, &FileBrowserView::onDirectoryListFailed);
    connect(m_session, &SmbSession::fileStatted,
            this, &FileBrowserView::onFileStatted);
    connect(m_session, &SmbSession::statFailed,
            this, &FileBrowserView::onStatFailed);

    // Scrolling changes which rows are visible; freed-up slots should go to
    // them first.
    connect(m_tree->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this] { pumpStats(); });

    updateCountLabel();
}

QList<SelectedFile> FileBrowserView::selectedFiles() const
{
    QList<SelectedFile> files;
    const QModelIndexList rows = m_tree->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : rows) {
        const FileEntry &entry = m_model->entryAt(m_proxy->mapToSource(proxyIndex).row());
        if (!entry.isDir) {
            files.append({entryPath(entry.name), entry});
        }
    }
    return files;
}

void FileBrowserView::refresh()
{
    if (!m_currentPath.isEmpty()) {
        navigateTo(m_currentPath);
    }
}

void FileBrowserView::navigateTo(const QString &path)
{
    const QString normalized = normalizePath(path);
    m_pendingPath = normalized;
    m_pathEdit->setText(normalized);
    m_session->listDirectory(normalized);
}

void FileBrowserView::onPathEdited()
{
    navigateTo(m_pathEdit->text());
}

void FileBrowserView::onEntryActivated(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid()) {
        return;
    }
    const FileEntry &entry = m_model->entryAt(m_proxy->mapToSource(proxyIndex).row());
    if (entry.isDir) {
        navigateTo(entryPath(entry.name));
    }
}

void FileBrowserView::onDirectoryListed(const QString &path, const QList<FileEntry> &entries)
{
    if (path != m_pendingPath) {
        return; // a reply for a path this view no longer wants
    }
    m_pendingPath.clear();
    m_currentPath = path;
    m_pathEdit->setText(path);
    m_upButton->setEnabled(path != QLatin1String("/"));
    m_model->setEntries(entries);
    updateCountLabel();
    resetStatQueue();
    // The model reset cleared the selection without a selectionChanged signal
    // (QItemSelectionModel::reset is documented not to emit); tell listeners.
    emit selectionChanged();
}

void FileBrowserView::onDirectoryListFailed(const QString &path, const QString &message)
{
    if (path != m_pendingPath) {
        return;
    }
    m_pendingPath.clear();
    // Keep showing the last good listing; put its path back in the box.
    m_pathEdit->setText(m_currentPath.isEmpty() ? QStringLiteral("/") : m_currentPath);
    emit errorOccurred(message);
}

void FileBrowserView::onFileStatted(const QString &path, int nlink, quint64 inode)
{
    Q_UNUSED(inode);
    const auto it = m_statInFlight.constFind(path);
    if (it == m_statInFlight.constEnd()) {
        return; // stale: a reply for a directory we've navigated away from
    }
    const int row = it.value();
    m_statInFlight.erase(it);
    m_model->setNlink(row, nlink);
    pumpStats();
}

// A failed stat (e.g. no permission) shows as "?" — no status-bar noise, a
// whole directory of them would flood it.
void FileBrowserView::onStatFailed(const QString &path, const QString &message)
{
    Q_UNUSED(message);
    const auto it = m_statInFlight.constFind(path);
    if (it == m_statInFlight.constEnd()) {
        return;
    }
    const int row = it.value();
    m_statInFlight.erase(it);
    m_model->setNlink(row, FileEntry::kNlinkUnavailable);
    pumpStats();
}

QString FileBrowserView::entryPath(const QString &name) const
{
    return m_currentPath == QLatin1String("/")
               ? QLatin1Char('/') + name
               : m_currentPath + QLatin1Char('/') + name;
}

void FileBrowserView::resetStatQueue()
{
    // In-flight replies for the old directory now miss m_statInFlight and are
    // dropped; the window may transiently hold old + new requests, bounded by
    // 2 * kMaxStatsInFlight.
    m_statInFlight.clear();
    m_statRequested.clear();
    m_statOrder.clear();
    m_statCursor = 0;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        if (!m_model->entryAt(row).isDir) {
            m_statOrder.append(row);
        }
    }
    pumpStats();
}

void FileBrowserView::pumpStats()
{
    while (m_statInFlight.size() < kMaxStatsInFlight) {
        const int row = nextStatRow();
        if (row < 0) {
            return;
        }
        const QString path = entryPath(m_model->entryAt(row).name);
        m_statRequested.insert(row);
        m_statInFlight.insert(path, row);
        m_session->statFile(path);
    }
}

// Picks the next row needing a stat: visible rows first, then listing order.
int FileBrowserView::nextStatRow()
{
    const int viewportBottom = m_tree->viewport()->height();
    QModelIndex idx = m_tree->indexAt(QPoint(0, 0));
    while (idx.isValid() && m_tree->visualRect(idx).top() < viewportBottom) {
        const int row = m_proxy->mapToSource(idx).row();
        const FileEntry &entry = m_model->entryAt(row);
        if (!entry.isDir && entry.nlink == FileEntry::kNlinkUnknown
            && !m_statRequested.contains(row)) {
            return row;
        }
        idx = m_tree->indexBelow(idx);
    }

    while (m_statCursor < m_statOrder.size()) {
        const int row = m_statOrder.at(m_statCursor);
        if (!m_statRequested.contains(row)) {
            return row;
        }
        ++m_statCursor;
    }
    return -1;
}

void FileBrowserView::updateCountLabel()
{
    const int total = m_model->rowCount();
    m_countLabel->setText(m_filterEdit->text().isEmpty()
                              ? QString::number(total)
                              : tr("%1 / %2").arg(m_proxy->rowCount()).arg(total));
}

// Cleans up hand-typed paths: guarantees a leading '/', collapses "//", "."
// and "..", drops trailing slashes.
QString FileBrowserView::normalizePath(const QString &path)
{
    QString p = path.trimmed();
    if (!p.startsWith(QLatin1Char('/'))) {
        p.prepend(QLatin1Char('/'));
    }
    p = QDir::cleanPath(p);
    return p.isEmpty() ? QStringLiteral("/") : p;
}
