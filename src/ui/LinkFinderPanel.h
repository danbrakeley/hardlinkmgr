#pragma once

#include <QString>
#include <QWidget>

#include "core/LinkSearcher.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QModelIndex;
class QPushButton;
class QSortFilterProxyModel;
class QSpinBox;
class QTreeView;

class LinkResultsModel;
class SmbSession;

// The Link Finder: options for a recursive search over a single folder,
// grouping files that already share an inode (existing hard-link groups),
// and a sortable results table. Search + browse only — no linking action
// (that's a separate, lower-priority roadmap item). Options persist in
// QSettings; the search path is validated when Start Search is clicked
// (see LinkSearcher), not on connect.
class LinkFinderPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LinkFinderPanel(SmbSession *session, QWidget *parent = nullptr);

signals:
    // A result row was selected; the file view should show it.
    void revealRequested(const QString &folder, const QString &name);
    void statusMessage(const QString &message); // for the main status bar

    // The search just started or just stopped (finished/failed/cancelled).
    // MainWindow pauses every view's lazy stat pump while any search runs,
    // since they share one SmbSession.
    void searchRunningChanged(bool running);

private:
    void loadSettings();
    void wireSettingsSaves();
    void onStartClicked();
    void onSearchProgress(int foldersListed, int foldersPending, int filesGathered);
    void onSearchFinished(const QList<LinkSearcher::Entry> &entries,
                          int folderErrors, bool truncated, bool cancelled);
    void onSearchFailed(const QString &message);
    void onCurrentRowChanged(const QModelIndex &current);
    void updateControls();

    SmbSession *m_session = nullptr;
    LinkSearcher *m_searcher = nullptr;
    LinkResultsModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;

    QWidget *m_optionsForm = nullptr; // the inputs; disabled during a search
    QLineEdit *m_searchPathEdit = nullptr;
    QCheckBox *m_recurseCheckbox = nullptr;
    QSpinBox *m_sizeMinValue = nullptr;
    QComboBox *m_sizeMinUnit = nullptr;
    QSpinBox *m_linkMinValue = nullptr;
    QPushButton *m_startButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTreeView *m_tree = nullptr;
    QLabel *m_resultsLabel = nullptr;
    bool m_lastSearching = false; // last value emitted via searchRunningChanged
};
