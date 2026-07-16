#include "FileBrowserView.h"

#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QStyle>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace {

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

    updateCountLabel();
}

QItemSelectionModel *FileBrowserView::selectionModel() const
{
    return m_tree->selectionModel();
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
        navigateTo(m_currentPath == QLatin1String("/")
                       ? QLatin1Char('/') + entry.name
                       : m_currentPath + QLatin1Char('/') + entry.name);
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
