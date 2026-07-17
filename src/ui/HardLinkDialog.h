#pragma once

#include <QDialog>
#include <QList>

#include "ui/FileBrowserView.h" // SelectedFile

class QButtonGroup;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class SmbSession;

// The Hard Link dialog (README): lists the selected files, requires choosing
// the primary to keep, then replaces each other file with a hard link to the
// primary. Per victim the sequence is
//     rename victim -> victim.hlmgr-tmp
//     link   primary -> victim's original path
//     unlink victim.hlmgr-tmp
// so the victim's data still exists (under the tmp name) until the link is in
// place; a failed link renames the tmp back. Victims run one at a time.
class HardLinkDialog : public QDialog
{
    Q_OBJECT

public:
    HardLinkDialog(SmbSession *session, const QList<SelectedFile> &files,
                   QWidget *parent = nullptr);

private:
    enum class Step { Rename, Link, Unlink, UndoRename };

    struct Victim
    {
        QString path;
        QString tmpPath;
        QTreeWidgetItem *item;
    };

    void startLinking();
    void startNextVictim();
    void onOpSucceeded(quint64 id);
    void onOpFailed(quint64 id, const QString &message);
    void setStatus(const Victim &victim, const QString &text);
    void finishRun();

    SmbSession *m_session = nullptr;
    QList<SelectedFile> m_files;
    QTreeWidget *m_list = nullptr;
    QButtonGroup *m_primaryGroup = nullptr;
    QPushButton *m_linkButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    QLabel *m_summaryLabel = nullptr;

    QList<Victim> m_victims;
    QString m_primaryPath;
    int m_current = -1;      // index into m_victims while running
    Step m_step = Step::Rename;
    quint64 m_opId = 0;      // id of the operation we are waiting for
    int m_replaced = 0;
    int m_failed = 0;
};
