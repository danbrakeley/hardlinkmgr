#include "MatchFinderPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QSpinBox>
#include <QTreeView>
#include <QVBoxLayout>

#include <limits>

#include "core/LinkRunner.h"
#include "core/MatchConflicts.h"
#include "core/PathUtil.h"
#include "models/MatchResultsModel.h"
#include "smb/SmbSession.h"
#include "ui/CheckBoxHeader.h"

namespace {

const auto kKeyPrimaryPath = QStringLiteral("matchfinder/primaryPath");
const auto kKeyPrimaryRecursive = QStringLiteral("matchfinder/primaryRecursive");
const auto kKeySecondaryPath = QStringLiteral("matchfinder/secondaryPath");
const auto kKeySecondaryRecursive = QStringLiteral("matchfinder/secondaryRecursive");
const auto kKeySizeMinValue = QStringLiteral("matchfinder/sizeMinValue");
const auto kKeySizeMinUnit = QStringLiteral("matchfinder/sizeMinUnit");
const auto kKeySizeDiffValue = QStringLiteral("matchfinder/sizeDiffValue");
const auto kKeySizeDiffUnit = QStringLiteral("matchfinder/sizeDiffUnit");

constexpr int kDefaultSizeMinValue = 10; // MiB
constexpr int kDefaultSizeDiffValue = 0; // MiB
constexpr int kUnitMiB = 2;              // index into the units dropdown

QComboBox *makeUnitCombo(QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    combo->addItems({QStringLiteral("bytes"), QStringLiteral("KiB"),
                     QStringLiteral("MiB"), QStringLiteral("GiB")});
    return combo;
}

QSpinBox *makeSizeSpin(QWidget *parent)
{
    auto *spin = new QSpinBox(parent);
    spin->setRange(0, std::numeric_limits<int>::max());
    return spin;
}

} // namespace

MatchFinderPanel::MatchFinderPanel(SmbSession *session, QWidget *parent)
    : QWidget(parent)
    , m_session(session)
{
    m_searcher = new MatchSearcher(m_session, this);
    m_runner = new LinkRunner(m_session, this);
    m_model = new MatchResultsModel(this);

    // --- Match Finder Options ---
    auto *optionsGroup = new QGroupBox(tr("Match Finder Options"), this);

    m_optionsForm = new QWidget(optionsGroup);
    m_primaryPathEdit = new QLineEdit(m_optionsForm);
    m_primaryPathEdit->setObjectName(QStringLiteral("mfp.primaryPath"));
    m_primaryPathEdit->setPlaceholderText(QStringLiteral("/path/to/folder"));
    m_primaryRecurse = new QCheckBox(tr("Include Subfolders"), m_optionsForm);
    m_primaryRecurse->setObjectName(QStringLiteral("mfp.primaryRecurse"));
    m_secondaryPathEdit = new QLineEdit(m_optionsForm);
    m_secondaryPathEdit->setObjectName(QStringLiteral("mfp.secondaryPath"));
    m_secondaryPathEdit->setPlaceholderText(QStringLiteral("/path/to/folder"));
    m_secondaryRecurse = new QCheckBox(tr("Include Subfolders"), m_optionsForm);
    m_secondaryRecurse->setObjectName(QStringLiteral("mfp.secondaryRecurse"));
    m_sizeMinValue = makeSizeSpin(m_optionsForm);
    m_sizeMinValue->setObjectName(QStringLiteral("mfp.sizeMinValue"));
    m_sizeMinUnit = makeUnitCombo(m_optionsForm);
    m_sizeMinUnit->setObjectName(QStringLiteral("mfp.sizeMinUnit"));
    m_sizeDiffValue = makeSizeSpin(m_optionsForm);
    m_sizeDiffValue->setObjectName(QStringLiteral("mfp.sizeDiffValue"));
    m_sizeDiffUnit = makeUnitCombo(m_optionsForm);
    m_sizeDiffUnit->setObjectName(QStringLiteral("mfp.sizeDiffUnit"));

    auto *grid = new QGridLayout(m_optionsForm);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->addWidget(new QLabel(tr("Primary Path"), m_optionsForm), 0, 0);
    grid->addWidget(m_primaryPathEdit, 0, 1);
    grid->addWidget(m_primaryRecurse, 0, 2);
    grid->addWidget(new QLabel(tr("Secondary Path"), m_optionsForm), 1, 0);
    grid->addWidget(m_secondaryPathEdit, 1, 1);
    grid->addWidget(m_secondaryRecurse, 1, 2);
    auto *sizeMinLayout = new QHBoxLayout;
    sizeMinLayout->addWidget(m_sizeMinValue);
    sizeMinLayout->addWidget(m_sizeMinUnit);
    sizeMinLayout->addStretch();
    grid->addWidget(new QLabel(tr("Size Min"), m_optionsForm), 2, 0);
    grid->addLayout(sizeMinLayout, 2, 1, 1, 2);
    auto *sizeDiffLayout = new QHBoxLayout;
    sizeDiffLayout->addWidget(m_sizeDiffValue);
    sizeDiffLayout->addWidget(m_sizeDiffUnit);
    sizeDiffLayout->addStretch();
    grid->addWidget(new QLabel(tr("Size Difference"), m_optionsForm), 3, 0);
    grid->addLayout(sizeDiffLayout, 3, 1, 1, 2);
    grid->setColumnStretch(1, 1);

    m_statusLabel = new QLabel(optionsGroup);
    m_statusLabel->setObjectName(QStringLiteral("mfp.statusLabel"));
    m_statusLabel->setWordWrap(true);

    m_startButton = new QPushButton(tr("Start Search"), optionsGroup);
    m_startButton->setObjectName(QStringLiteral("mfp.startButton"));
    connect(m_startButton, &QPushButton::clicked,
            this, &MatchFinderPanel::onStartClicked);

    auto *startRow = new QHBoxLayout;
    startRow->addWidget(m_statusLabel, /*stretch*/ 1);
    startRow->addWidget(m_startButton);

    auto *optionsLayout = new QVBoxLayout(optionsGroup);
    optionsLayout->addWidget(m_optionsForm);
    optionsLayout->addLayout(startRow);

    // --- Match Finder Results ---
    auto *resultsGroup = new QGroupBox(tr("Match Finder Results"), this);

    m_tree = new QTreeView(resultsGroup);
    m_tree->setObjectName(QStringLiteral("mfp.tree"));
    m_header = new CheckBoxHeader(Qt::Horizontal, m_tree);
    m_tree->setHeader(m_header);
    m_tree->setModel(m_model);
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setAllColumnsShowFocus(true);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_header->setStretchLastSection(false);
    m_header->setSectionResizeMode(MatchResultsModel::CheckColumn, QHeaderView::Fixed);
    m_header->resizeSection(MatchResultsModel::CheckColumn, 28);
    m_header->setSectionResizeMode(MatchResultsModel::PrimaryColumn, QHeaderView::Stretch);
    m_header->setSectionResizeMode(MatchResultsModel::SecondaryColumn, QHeaderView::Stretch);
    m_header->setSectionResizeMode(MatchResultsModel::StatusColumn, QHeaderView::Stretch);

    m_linkButton = new QPushButton(tr("Link Selected Matches"), resultsGroup);
    m_linkButton->setObjectName(QStringLiteral("mfp.linkButton"));
    connect(m_linkButton, &QPushButton::clicked,
            this, &MatchFinderPanel::onLinkSelectedClicked);

    auto *linkRow = new QHBoxLayout;
    linkRow->addStretch();
    linkRow->addWidget(m_linkButton);

    auto *resultsLayout = new QVBoxLayout(resultsGroup);
    resultsLayout->addWidget(m_tree, /*stretch*/ 1);
    resultsLayout->addLayout(linkRow);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(optionsGroup);
    layout->addWidget(resultsGroup, /*stretch*/ 1);

    // --- wiring ---
    connect(m_searcher, &MatchSearcher::progress,
            this, &MatchFinderPanel::onSearchProgress);
    connect(m_searcher, &MatchSearcher::finished,
            this, &MatchFinderPanel::onSearchFinished);
    connect(m_searcher, &MatchSearcher::failed,
            this, &MatchFinderPanel::onSearchFailed);

    connect(m_session, &SmbSession::directoryListed,
            this, &MatchFinderPanel::onDirectoryListed);
    connect(m_session, &SmbSession::directoryListFailed,
            this, &MatchFinderPanel::onDirectoryListFailed);

    connect(m_tree->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex &current, const QModelIndex &) {
        onCurrentRowChanged(current);
    });

    connect(m_header, &CheckBoxHeader::toggleAllRequested,
            m_model, &MatchResultsModel::setAllChecked);
    connect(m_model, &MatchResultsModel::checkedCountChanged, this, [this] {
        m_header->setCheckState(m_model->aggregateCheckState());
        updateControls();
    });

    connect(m_runner, &LinkRunner::jobStatusChanged,
            this, [this](int job, const QString &text) {
        m_model->setStatus(m_jobRows.at(job), text);
    });
    connect(m_runner, &LinkRunner::jobFinished,
            this, [this](int job, bool /*success*/, const QString &message) {
        m_model->setStatus(m_jobRows.at(job), message);
    });
    connect(m_runner, &LinkRunner::allFinished, this, [this](int succeeded, int failed) {
        m_model->setLocked(false);
        updateControls();
        emit statusMessage(failed == 0
                               ? tr("Replaced %1 file(s) with hard links.").arg(succeeded)
                               : tr("Replaced %1 file(s); %2 failed — see the Status column.")
                                     .arg(succeeded).arg(failed));
        emit linkRunFinished();
    });

    loadSettings();
    wireSettingsSaves();
    updateControls();
}

void MatchFinderPanel::loadSettings()
{
    const QSettings settings;
    m_primaryPathEdit->setText(settings.value(kKeyPrimaryPath).toString());
    m_primaryRecurse->setChecked(settings.value(kKeyPrimaryRecursive, true).toBool());
    m_secondaryPathEdit->setText(settings.value(kKeySecondaryPath).toString());
    m_secondaryRecurse->setChecked(settings.value(kKeySecondaryRecursive, true).toBool());
    m_sizeMinValue->setValue(settings.value(kKeySizeMinValue, kDefaultSizeMinValue).toInt());
    m_sizeMinUnit->setCurrentIndex(settings.value(kKeySizeMinUnit, kUnitMiB).toInt());
    m_sizeDiffValue->setValue(settings.value(kKeySizeDiffValue, kDefaultSizeDiffValue).toInt());
    m_sizeDiffUnit->setCurrentIndex(settings.value(kKeySizeDiffUnit, kUnitMiB).toInt());
}

// Every option writes through on change (matching connect/lastUrl's
// write-on-event style), so nothing is lost on a crash.
void MatchFinderPanel::wireSettingsSaves()
{
    connect(m_primaryPathEdit, &QLineEdit::editingFinished, this, [this] {
        QSettings().setValue(kKeyPrimaryPath, m_primaryPathEdit->text().trimmed());
    });
    connect(m_secondaryPathEdit, &QLineEdit::editingFinished, this, [this] {
        QSettings().setValue(kKeySecondaryPath, m_secondaryPathEdit->text().trimmed());
    });
    connect(m_primaryRecurse, &QCheckBox::toggled, this, [](bool on) {
        QSettings().setValue(kKeyPrimaryRecursive, on);
    });
    connect(m_secondaryRecurse, &QCheckBox::toggled, this, [](bool on) {
        QSettings().setValue(kKeySecondaryRecursive, on);
    });
    connect(m_sizeMinValue, &QSpinBox::valueChanged, this, [](int value) {
        QSettings().setValue(kKeySizeMinValue, value);
    });
    connect(m_sizeMinUnit, &QComboBox::currentIndexChanged, this, [](int index) {
        QSettings().setValue(kKeySizeMinUnit, index);
    });
    connect(m_sizeDiffValue, &QSpinBox::valueChanged, this, [](int value) {
        QSettings().setValue(kKeySizeDiffValue, value);
    });
    connect(m_sizeDiffUnit, &QComboBox::currentIndexChanged, this, [](int index) {
        QSettings().setValue(kKeySizeDiffUnit, index);
    });
}

quint64 MatchFinderPanel::byteValue(const QSpinBox *value, const QComboBox *unit)
{
    return quint64(value->value()) << (10 * unit->currentIndex());
}

void MatchFinderPanel::beginPathValidation()
{
    const QString primary = m_primaryPathEdit->text().trimmed();
    const QString secondary = m_secondaryPathEdit->text().trimmed();
    // Members are set before the listDirectory calls: a synchronous failure
    // re-enters onDirectoryListFailed immediately.
    m_primaryValidating = primary.isEmpty() ? QString() : pathutil::normalize(primary);
    m_secondaryValidating = secondary.isEmpty() ? QString() : pathutil::normalize(secondary);
    const QString first = m_primaryValidating;
    const QString second = m_secondaryValidating;
    if (!first.isEmpty()) {
        m_session->listDirectory(first);
    }
    if (!second.isEmpty() && second != first) {
        m_session->listDirectory(second);
    }
    updateControls();
}

void MatchFinderPanel::onDirectoryListed(const QString &path, const QList<FileEntry> &entries)
{
    Q_UNUSED(entries);
    bool cleared = false;
    if (path == m_primaryValidating) {
        m_primaryValidating.clear();
        cleared = true;
    }
    if (path == m_secondaryValidating) {
        m_secondaryValidating.clear();
        cleared = true;
    }
    if (cleared) {
        updateControls();
    }
}

void MatchFinderPanel::onDirectoryListFailed(const QString &path, const QString &message)
{
    Q_UNUSED(message);
    bool cleared = false;
    if (path == m_primaryValidating) {
        m_primaryValidating.clear();
        m_primaryPathEdit->clear();
        QSettings().setValue(kKeyPrimaryPath, QString());
        cleared = true;
    }
    if (path == m_secondaryValidating) {
        m_secondaryValidating.clear();
        m_secondaryPathEdit->clear();
        QSettings().setValue(kKeySecondaryPath, QString());
        cleared = true;
    }
    if (cleared) {
        emit statusMessage(tr("Match Finder: saved path %1 was not found on this "
                              "server and has been cleared.").arg(path));
        updateControls();
    }
}

void MatchFinderPanel::onStartClicked()
{
    if (m_searcher->isRunning()) {
        m_searcher->cancel();
        return;
    }

    const QString primary = m_primaryPathEdit->text().trimmed();
    const QString secondary = m_secondaryPathEdit->text().trimmed();
    if (primary.isEmpty() || secondary.isEmpty()) {
        m_statusLabel->setText(tr("Enter both a Primary and a Secondary path."));
        return;
    }

    MatchSearcher::Options options;
    options.primaryPath = pathutil::normalize(primary);
    options.secondaryPath = pathutil::normalize(secondary);
    options.primaryRecursive = m_primaryRecurse->isChecked();
    options.secondaryRecursive = m_secondaryRecurse->isChecked();
    options.sizeMinBytes = byteValue(m_sizeMinValue, m_sizeMinUnit);
    options.sizeDiffBytes = byteValue(m_sizeDiffValue, m_sizeDiffUnit);

    m_model->setMatches({});
    m_statusLabel->setText(tr("Searching…"));
    m_searcher->start(options);
    updateControls();
}

void MatchFinderPanel::onSearchProgress(int foldersListed, int foldersPending, int filesGathered)
{
    m_statusLabel->setText(tr("Searching… %1 folders listed (%2 pending), %3 candidate files")
                               .arg(foldersListed).arg(foldersPending).arg(filesGathered));
}

void MatchFinderPanel::onSearchFinished(const QList<MatchSearcher::Match> &matches,
                                        int folderErrors, bool truncated, bool cancelled)
{
    updateControls();
    if (cancelled) {
        m_statusLabel->setText(tr("Search cancelled."));
        return;
    }
    m_model->setMatches(matches);
    QString text = tr("%1 potential match(es).").arg(matches.size());
    if (folderErrors > 0) {
        text += tr(" %1 folder(s) could not be listed.").arg(folderErrors);
    }
    if (truncated) {
        text += tr(" Results were truncated — narrow the search.");
    }
    m_statusLabel->setText(text);
}

void MatchFinderPanel::onSearchFailed(const QString &message)
{
    updateControls();
    m_statusLabel->setText(message);
}

void MatchFinderPanel::onCurrentRowChanged(const QModelIndex &current)
{
    if (!current.isValid()) {
        return;
    }
    const MatchSearcher::Match &match = m_model->matchAt(current.row());
    emit revealRequested(pathutil::folderOf(match.primaryPath),
                         pathutil::nameOf(match.primaryPath),
                         pathutil::folderOf(match.secondaryPath),
                         pathutil::nameOf(match.secondaryPath));
}

void MatchFinderPanel::onLinkSelectedClicked()
{
    if (m_searcher->isRunning() || m_runner->isRunning()) {
        return;
    }
    const QList<int> rows = m_model->checkedRows();
    if (rows.isEmpty()) {
        return;
    }

    QList<MatchSearcher::Match> checkedMatches;
    checkedMatches.reserve(rows.size());
    for (int row : rows) {
        checkedMatches.append(m_model->matchAt(row));
    }
    const matchconflicts::Conflicts found = matchconflicts::find(checkedMatches);
    if (!found.isEmpty()) {
        QStringList conflicts;
        for (const QString &path : found.duplicateSecondaries) {
            conflicts.append(tr("%1 is the secondary of more than one checked row.")
                                 .arg(path));
        }
        for (const QString &path : found.primaryAndSecondary) {
            conflicts.append(tr("%1 is both a primary and a secondary among the "
                                "checked rows.").arg(path));
        }
        QMessageBox::warning(this, tr("Conflicting Selections"),
                             tr("Resolve these conflicts (uncheck rows) first:\n\n%1")
                                 .arg(conflicts.join(QLatin1Char('\n'))));
        return;
    }

    if (QMessageBox::question(this, tr("Link Selected Matches"),
                              tr("Replace %1 file(s) with hard links?").arg(rows.size()))
        != QMessageBox::Yes) {
        return;
    }

    m_jobRows = rows;
    QList<LinkRunner::Job> jobs;
    jobs.reserve(rows.size());
    for (int row : rows) {
        const MatchSearcher::Match &match = m_model->matchAt(row);
        jobs.append({match.primaryPath, match.secondaryPath});
    }
    m_model->setLocked(true);
    m_runner->start(jobs);
    updateControls();
}

void MatchFinderPanel::updateControls()
{
    const bool searching = m_searcher->isRunning();
    const bool linking = m_runner->isRunning();
    m_startButton->setText(searching ? tr("Cancel Search") : tr("Start Search"));
    m_startButton->setEnabled(searching || (!linking && !validationPending()));
    m_linkButton->setEnabled(!searching && !linking && m_model->checkedCount() > 0);
    m_optionsForm->setEnabled(!searching && !linking);
}
