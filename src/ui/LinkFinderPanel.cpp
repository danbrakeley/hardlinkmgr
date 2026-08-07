#include "LinkFinderPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QTreeView>
#include <QVBoxLayout>

#include <limits>

#include "core/PathUtil.h"
#include "models/LinkResultsModel.h"
#include "smb/SmbSession.h"
#include "ui/SizeUnitWidgets.h"

namespace {

const auto kKeySearchPath = QStringLiteral("linkfinder/searchPath");
const auto kKeyRecursive = QStringLiteral("linkfinder/recursive");
const auto kKeySizeMinValue = QStringLiteral("linkfinder/sizeMinValue");
const auto kKeySizeMinUnit = QStringLiteral("linkfinder/sizeMinUnit");
const auto kKeyLinkMinValue = QStringLiteral("linkfinder/linkMinValue");

constexpr int kDefaultSizeMinValue = 0;
constexpr int kDefaultSizeMinUnit = 0; // index into the units dropdown: bytes
constexpr int kDefaultLinkMinValue = 2;

} // namespace

LinkFinderPanel::LinkFinderPanel(SmbSession *session, QWidget *parent)
    : QWidget(parent)
    , m_session(session)
{
    m_searcher = new LinkSearcher(m_session, this);
    m_model = new LinkResultsModel(this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);

    // --- Link Finder Options ---
    auto *optionsGroup = new QGroupBox(tr("Link Finder Options"), this);

    m_optionsForm = new QWidget(optionsGroup);
    m_searchPathEdit = new QLineEdit(m_optionsForm);
    m_searchPathEdit->setObjectName(QStringLiteral("lfp.searchPath"));
    m_searchPathEdit->setPlaceholderText(QStringLiteral("/path/to/folder"));
    m_recurseCheckbox = new QCheckBox(tr("Include Subfolders"), m_optionsForm);
    m_recurseCheckbox->setObjectName(QStringLiteral("lfp.recurse"));
    m_sizeMinValue = sizeunits::makeSizeSpin(m_optionsForm);
    m_sizeMinValue->setObjectName(QStringLiteral("lfp.sizeMinValue"));
    m_sizeMinUnit = sizeunits::makeUnitCombo(m_optionsForm);
    m_sizeMinUnit->setObjectName(QStringLiteral("lfp.sizeMinUnit"));
    m_linkMinValue = new QSpinBox(m_optionsForm);
    m_linkMinValue->setObjectName(QStringLiteral("lfp.linkMinValue"));
    m_linkMinValue->setRange(0, std::numeric_limits<int>::max());

    auto *grid = new QGridLayout(m_optionsForm);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->addWidget(new QLabel(tr("Search Path"), m_optionsForm), 0, 0);
    grid->addWidget(m_searchPathEdit, 0, 1);
    grid->addWidget(m_recurseCheckbox, 0, 2);
    auto *sizeMinLayout = new QHBoxLayout;
    sizeMinLayout->addWidget(m_sizeMinValue);
    sizeMinLayout->addWidget(m_sizeMinUnit);
    sizeMinLayout->addStretch();
    grid->addWidget(new QLabel(tr("Size Min"), m_optionsForm), 1, 0);
    grid->addLayout(sizeMinLayout, 1, 1, 1, 2);
    auto *linkMinLayout = new QHBoxLayout;
    linkMinLayout->addWidget(m_linkMinValue);
    linkMinLayout->addStretch();
    grid->addWidget(new QLabel(tr("Link Min"), m_optionsForm), 2, 0);
    grid->addLayout(linkMinLayout, 2, 1, 1, 2);
    grid->setColumnStretch(1, 1);

    m_statusLabel = new QLabel(optionsGroup);
    m_statusLabel->setObjectName(QStringLiteral("lfp.statusLabel"));
    m_statusLabel->setWordWrap(true);

    m_startButton = new QPushButton(tr("Start Search"), optionsGroup);
    m_startButton->setObjectName(QStringLiteral("lfp.startButton"));
    connect(m_startButton, &QPushButton::clicked,
            this, &LinkFinderPanel::onStartClicked);

    auto *startRow = new QHBoxLayout;
    startRow->addWidget(m_statusLabel, /*stretch*/ 1);
    startRow->addWidget(m_startButton);

    auto *optionsLayout = new QVBoxLayout(optionsGroup);
    optionsLayout->addWidget(m_optionsForm);
    optionsLayout->addLayout(startRow);

    // --- Link Finder Results ---
    auto *resultsGroup = new QGroupBox(tr("Link Finder Results"), this);

    m_tree = new QTreeView(resultsGroup);
    m_tree->setObjectName(QStringLiteral("lfp.tree"));
    m_tree->setModel(m_proxy);
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setAllColumnsShowFocus(true);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(LinkResultsModel::LinksColumn, Qt::DescendingOrder);
    QHeaderView *header = m_tree->header();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(LinkResultsModel::NameColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(LinkResultsModel::PathColumn, QHeaderView::Stretch);
    // Links is almost always a single digit; Inode should fit a 7-digit
    // number without immediately needing a manual resize.
    header->resizeSection(LinkResultsModel::LinksColumn,
                          header->fontMetrics().horizontalAdvance(tr("Links")) + 24);
    header->resizeSection(LinkResultsModel::InodeColumn,
                          header->fontMetrics().horizontalAdvance(QStringLiteral("0000000")) + 24);

    m_resultsLabel = new QLabel(resultsGroup);
    m_resultsLabel->setObjectName(QStringLiteral("lfp.resultsLabel"));

    auto *resultsLayout = new QVBoxLayout(resultsGroup);
    resultsLayout->addWidget(m_tree, /*stretch*/ 1);
    resultsLayout->addWidget(m_resultsLabel);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(optionsGroup);
    layout->addWidget(resultsGroup, /*stretch*/ 1);

    // --- wiring ---
    connect(m_searcher, &LinkSearcher::progress,
            this, &LinkFinderPanel::onSearchProgress);
    connect(m_searcher, &LinkSearcher::finished,
            this, &LinkFinderPanel::onSearchFinished);
    connect(m_searcher, &LinkSearcher::failed,
            this, &LinkFinderPanel::onSearchFailed);

    connect(m_tree->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex &current, const QModelIndex &) {
        onCurrentRowChanged(current);
    });

    loadSettings();
    wireSettingsSaves();
    updateControls();
}

void LinkFinderPanel::loadSettings()
{
    const QSettings settings;
    m_searchPathEdit->setText(settings.value(kKeySearchPath, QStringLiteral("/")).toString());
    m_recurseCheckbox->setChecked(settings.value(kKeyRecursive, true).toBool());
    m_sizeMinValue->setValue(settings.value(kKeySizeMinValue, kDefaultSizeMinValue).toInt());
    m_sizeMinUnit->setCurrentIndex(settings.value(kKeySizeMinUnit, kDefaultSizeMinUnit).toInt());
    m_linkMinValue->setValue(settings.value(kKeyLinkMinValue, kDefaultLinkMinValue).toInt());
}

// Every option writes through on change (matching MatchFinderPanel's
// write-on-event style), so nothing is lost on a crash.
void LinkFinderPanel::wireSettingsSaves()
{
    connect(m_searchPathEdit, &QLineEdit::editingFinished, this, [this] {
        QSettings().setValue(kKeySearchPath, m_searchPathEdit->text().trimmed());
    });
    connect(m_recurseCheckbox, &QCheckBox::toggled, this, [](bool on) {
        QSettings().setValue(kKeyRecursive, on);
    });
    connect(m_sizeMinValue, &QSpinBox::valueChanged, this, [](int value) {
        QSettings().setValue(kKeySizeMinValue, value);
    });
    connect(m_sizeMinUnit, &QComboBox::currentIndexChanged, this, [](int index) {
        QSettings().setValue(kKeySizeMinUnit, index);
    });
    connect(m_linkMinValue, &QSpinBox::valueChanged, this, [](int value) {
        QSettings().setValue(kKeyLinkMinValue, value);
    });
}

void LinkFinderPanel::onStartClicked()
{
    if (m_searcher->isRunning()) {
        m_searcher->cancel();
        return;
    }

    const QString searchPath = m_searchPathEdit->text().trimmed();
    if (searchPath.isEmpty()) {
        m_statusLabel->setText(tr("Enter a Search Path."));
        return;
    }

    LinkSearcher::Options options;
    options.searchPath = pathutil::normalize(searchPath);
    options.recursive = m_recurseCheckbox->isChecked();
    options.sizeMinBytes = sizeunits::byteValue(m_sizeMinValue, m_sizeMinUnit);
    options.linkMinCount = m_linkMinValue->value();

    m_model->setResults({});
    m_resultsLabel->clear();
    m_statusLabel->setText(tr("Searching…"));
    m_searcher->start(options);
    updateControls();
}

void LinkFinderPanel::onSearchProgress(int foldersListed, int foldersPending, int filesGathered)
{
    m_statusLabel->setText(tr("Searching… %1 folders listed (%2 pending), %3 candidate files")
                               .arg(foldersListed).arg(foldersPending).arg(filesGathered));
}

void LinkFinderPanel::onSearchFinished(const QList<LinkSearcher::Entry> &entries,
                                       int folderErrors, bool truncated, bool cancelled)
{
    updateControls();
    if (cancelled) {
        m_statusLabel->setText(tr("Search cancelled."));
        return;
    }
    m_model->setResults(entries);
    m_resultsLabel->setText(tr("%1 result(s).").arg(entries.size()));
    QString text = tr("Search complete.");
    if (folderErrors > 0) {
        text += tr(" %1 folder(s) could not be listed.").arg(folderErrors);
    }
    if (truncated) {
        text += tr(" Results were truncated — narrow the search.");
    }
    m_statusLabel->setText(text);
}

void LinkFinderPanel::onSearchFailed(const QString &message)
{
    updateControls();
    m_statusLabel->setText(message);
}

void LinkFinderPanel::onCurrentRowChanged(const QModelIndex &current)
{
    if (!current.isValid()) {
        return;
    }
    const linkgrouping::Entry &entry = m_model->entryAt(m_proxy->mapToSource(current).row());
    emit revealRequested(pathutil::folderOf(entry.path), pathutil::nameOf(entry.path));
}

void LinkFinderPanel::updateControls()
{
    const bool searching = m_searcher->isRunning();
    m_startButton->setText(searching ? tr("Cancel Search") : tr("Start Search"));
    m_optionsForm->setEnabled(!searching);

    if (searching != m_lastSearching) {
        m_lastSearching = searching;
        emit searchRunningChanged(searching);
    }
}
