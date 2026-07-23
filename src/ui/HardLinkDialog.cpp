#include "HardLinkDialog.h"

#include <QButtonGroup>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QRadioButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "core/LinkRunner.h"

namespace {

constexpr int kKeepColumn = 0;
constexpr int kFileColumn = 1;
constexpr int kInfoColumn = 2;
constexpr int kStatusColumn = 3;

} // namespace

HardLinkDialog::HardLinkDialog(SmbSession *session, const QList<SelectedFile> &files,
                               QWidget *parent)
    : QDialog(parent)
    , m_files(files)
{
    setWindowTitle(tr("Create Hard Links"));

    m_runner = new LinkRunner(session, this);

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

    connect(m_runner, &LinkRunner::jobStatusChanged, this, [this](int job, const QString &text) {
        m_jobItems.at(job)->setText(kStatusColumn, text);
    });
    connect(m_runner, &LinkRunner::jobFinished,
            this, [this](int job, bool /*success*/, const QString &message) {
        m_jobItems.at(job)->setText(kStatusColumn, message);
    });
    connect(m_runner, &LinkRunner::allFinished, this, &HardLinkDialog::finishRun);

    resize(720, 320);
}

void HardLinkDialog::startLinking()
{
    const int primary = m_primaryGroup->checkedId();
    if (primary < 0 || m_runner->isRunning()) {
        return;
    }
    const QString primaryPath = m_files.at(primary).path;

    QList<LinkRunner::Job> jobs;
    m_jobItems.clear();
    for (int i = 0; i < m_files.size(); ++i) {
        QTreeWidgetItem *item = m_list->topLevelItem(i);
        if (i == primary) {
            item->setText(kStatusColumn, tr("kept (primary)"));
        } else {
            jobs.append({primaryPath, m_files.at(i).path});
            m_jobItems.append(item);
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
    m_runner->start(jobs);
}

void HardLinkDialog::finishRun(int replaced, int failed)
{
    m_summaryLabel->setText(failed == 0
                                ? tr("Replaced %1 file(s) with hard links.").arg(replaced)
                                : tr("Replaced %1 file(s); %2 failed — see Status.")
                                      .arg(replaced).arg(failed));
    m_closeButton->setText(tr("Close"));
    m_closeButton->setEnabled(true);
    // The run happened; make exec() report it so the views get refreshed.
    disconnect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
}
