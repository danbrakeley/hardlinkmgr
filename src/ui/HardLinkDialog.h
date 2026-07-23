#pragma once

#include <QDialog>
#include <QList>

#include "ui/FileBrowserView.h" // SelectedFile

class QButtonGroup;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class LinkRunner;
class SmbSession;

// The Hard Link dialog: lists the selected files, requires choosing
// the primary to keep, then replaces each other file with a hard link to the
// primary via LinkRunner (see LinkRunner.h for the per-victim sequence).
class HardLinkDialog : public QDialog
{
    Q_OBJECT

public:
    HardLinkDialog(SmbSession *session, const QList<SelectedFile> &files,
                   QWidget *parent = nullptr);

private:
    void startLinking();
    void finishRun(int replaced, int failed);

    QList<SelectedFile> m_files;
    QTreeWidget *m_list = nullptr;
    QButtonGroup *m_primaryGroup = nullptr;
    QPushButton *m_linkButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    QLabel *m_summaryLabel = nullptr;
    LinkRunner *m_runner = nullptr;
    QList<QTreeWidgetItem *> m_jobItems; // job index -> that victim's row
};
