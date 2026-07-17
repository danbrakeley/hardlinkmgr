#include "HardLinkDialog.h"

#include <QButtonGroup>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QRadioButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "smb/SmbSession.h"

namespace {

constexpr int kKeepColumn = 0;
constexpr int kFileColumn = 1;
constexpr int kInfoColumn = 2;
constexpr int kStatusColumn = 3;

const QLatin1String kTmpSuffix(".hlmgr-tmp");

} // namespace

HardLinkDialog::HardLinkDialog(SmbSession *session, const QList<SelectedFile> &files,
                               QWidget *parent)
    : QDialog(parent)
    , m_session(session)
    , m_files(files)
{
    setWindowTitle(tr("Create Hard Links"));

    auto *intro = new QLabel(
        tr("Choose the primary file to keep. Every other file below will be "
           "replaced by a hard link to it."),
        this);
    intro->setWordWrap(true);

    m_list = new QTreeWidget(this);
    m_list->setColumnCount(4);
    m_list->setHeaderLabels({tr("Keep"), tr("File"), tr("Size / Links / Inode"), tr("Status")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);

    m_primaryGroup = new QButtonGroup(this);
    m_primaryGroup->setExclusive(true);

    const QLocale locale;
    for (int i = 0; i < m_files.size(); ++i) {
        const SelectedFile &file = m_files.at(i);
        auto *item = new QTreeWidgetItem(m_list);
        item->setText(kFileColumn, file.path);
        const QString links = file.entry.nlink >= 0 ? QString::number(file.entry.nlink)
                                                    : QStringLiteral("?");
        item->setText(kInfoColumn, tr("%1 / %2 / %3")
                                       .arg(locale.formattedDataSize(qint64(file.entry.size)),
                                            links, QString::number(file.entry.inode)));
        auto *radio = new QRadioButton(m_list);
        m_primaryGroup->addButton(radio, i);
        m_list->setItemWidget(item, kKeepColumn, radio);
    }
    m_list->header()->setSectionResizeMode(kFileColumn, QHeaderView::ResizeToContents);
    connect(m_primaryGroup, &QButtonGroup::idClicked, this, [this](int) {
        m_linkButton->setEnabled(true);
    });

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);

    m_linkButton = new QPushButton(tr("Hard Link"), this);
    m_linkButton->setEnabled(false); // until a primary is chosen
    connect(m_linkButton, &QPushButton::clicked, this, &HardLinkDialog::startLinking);

    m_closeButton = new QPushButton(tr("Cancel"), this);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_summaryLabel, /*stretch*/ 1);
    buttons->addWidget(m_linkButton);
    buttons->addWidget(m_closeButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(m_list, /*stretch*/ 1);
    layout->addLayout(buttons);

    connect(m_session, &SmbSession::operationSucceeded,
            this, &HardLinkDialog::onOpSucceeded);
    connect(m_session, &SmbSession::operationFailed,
            this, &HardLinkDialog::onOpFailed);

    resize(720, 320);
}

void HardLinkDialog::startLinking()
{
    const int primary = m_primaryGroup->checkedId();
    if (primary < 0) {
        return;
    }
    m_primaryPath = m_files.at(primary).path;

    m_victims.clear();
    for (int i = 0; i < m_files.size(); ++i) {
        QTreeWidgetItem *item = m_list->topLevelItem(i);
        if (i == primary) {
            item->setText(kStatusColumn, tr("kept (primary)"));
        } else {
            m_victims.append({m_files.at(i).path,
                              m_files.at(i).path + kTmpSuffix,
                              item});
        }
        // No changing your mind mid-run.
        if (auto *radio = m_list->itemWidget(item, kKeepColumn)) {
            radio->setEnabled(false);
        }
    }

    // Mid-sequence cancel could strand a file under its tmp name, so the
    // dialog can only be closed between runs.
    m_linkButton->setEnabled(false);
    m_closeButton->setEnabled(false);
    m_current = -1;
    m_replaced = 0;
    m_failed = 0;
    startNextVictim();
}

void HardLinkDialog::startNextVictim()
{
    ++m_current;
    if (m_current >= m_victims.size()) {
        finishRun();
        return;
    }
    const Victim &victim = m_victims.at(m_current);
    setStatus(victim, tr("moving original aside…"));
    m_step = Step::Rename;
    m_opId = m_session->renameFile(victim.path, victim.tmpPath);
}

void HardLinkDialog::onOpSucceeded(quint64 id)
{
    if (id != m_opId) {
        return;
    }
    const Victim &victim = m_victims.at(m_current);

    switch (m_step) {
    case Step::Rename:
        setStatus(victim, tr("creating hard link…"));
        m_step = Step::Link;
        m_opId = m_session->createHardLink(m_primaryPath, victim.path);
        break;
    case Step::Link:
        setStatus(victim, tr("removing original…"));
        m_step = Step::Unlink;
        m_opId = m_session->removeFile(victim.tmpPath);
        break;
    case Step::Unlink:
        setStatus(victim, tr("replaced with hard link"));
        ++m_replaced;
        startNextVictim();
        break;
    case Step::UndoRename:
        // The failed-link status text is already in place; add the good news.
        setStatus(victim, victim.item->text(kStatusColumn) + tr(" (original restored)"));
        startNextVictim();
        break;
    }
}

void HardLinkDialog::onOpFailed(quint64 id, const QString &message)
{
    if (id != m_opId) {
        return;
    }
    const Victim &victim = m_victims.at(m_current);

    switch (m_step) {
    case Step::Rename:
        ++m_failed;
        setStatus(victim, tr("failed: %1").arg(message));
        startNextVictim();
        break;
    case Step::Link:
        // The original is intact under the tmp name; put it back.
        ++m_failed;
        setStatus(victim, tr("link failed: %1").arg(message));
        m_step = Step::UndoRename;
        m_opId = m_session->renameFile(victim.tmpPath, victim.path);
        break;
    case Step::Unlink:
        // The link exists; only the cleanup of the tmp copy failed.
        ++m_failed;
        setStatus(victim, tr("linked, but could not remove %1: %2")
                              .arg(victim.tmpPath, message));
        startNextVictim();
        break;
    case Step::UndoRename:
        setStatus(victim, victim.item->text(kStatusColumn)
                              + tr(" — RESTORE FAILED, original left at %1: %2")
                                    .arg(victim.tmpPath, message));
        startNextVictim();
        break;
    }
}

void HardLinkDialog::setStatus(const Victim &victim, const QString &text)
{
    victim.item->setText(kStatusColumn, text);
}

void HardLinkDialog::finishRun()
{
    m_summaryLabel->setText(m_failed == 0
                                ? tr("Replaced %1 file(s) with hard links.").arg(m_replaced)
                                : tr("Replaced %1 file(s); %2 failed — see Status.")
                                      .arg(m_replaced).arg(m_failed));
    m_closeButton->setText(tr("Close"));
    m_closeButton->setEnabled(true);
    // The run happened; make exec() report it so the views get refreshed.
    disconnect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
}
