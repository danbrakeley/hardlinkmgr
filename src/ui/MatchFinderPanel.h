#pragma once

#include <QList>
#include <QString>
#include <QWidget>

#include "core/MatchSearcher.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QModelIndex;
class QPushButton;
class QSpinBox;
class QTreeView;

class CheckBoxHeader;
class LinkRunner;
class MatchResultsModel;
class SmbSession;

// The Match Finder: options for a recursive potential-duplicate search over a
// primary and secondary folder, and the checkable results table whose rows can
// be turned into hard links ("Link Selected Matches"). Options persist in
// QSettings; MainWindow calls beginPathValidation() on connect so paths saved
// against a different server get cleared.
class MatchFinderPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MatchFinderPanel(SmbSession *session, QWidget *parent = nullptr);

    // Lists each saved path once; a path that fails is cleared (it belongs to
    // some other server). Start Search stays disabled until this resolves.
    void beginPathValidation();

signals:
    // A result row was selected; the file views should show both files.
    void revealRequested(const QString &primaryFolder, const QString &primaryName,
                         const QString &secondaryFolder, const QString &secondaryName);
    void linkRunFinished(); // files changed on the share; refresh the views
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
    void onSearchFinished(const QList<MatchSearcher::Match> &matches,
                          int folderErrors, bool truncated, bool cancelled);
    void onSearchFailed(const QString &message);
    void onCurrentRowChanged(const QModelIndex &current);
    void onLinkSelectedClicked();
    void onDirectoryListed(const QString &path, const QList<FileEntry> &entries);
    void onDirectoryListFailed(const QString &path, const QString &message);
    void updateControls();

    bool validationPending() const
    {
        return !m_primaryValidating.isEmpty() || !m_secondaryValidating.isEmpty();
    }

    SmbSession *m_session = nullptr;
    MatchSearcher *m_searcher = nullptr;
    LinkRunner *m_runner = nullptr;
    MatchResultsModel *m_model = nullptr;

    QWidget *m_optionsForm = nullptr; // the inputs; disabled during runs
    QLineEdit *m_primaryPathEdit = nullptr;
    QCheckBox *m_primaryRecurse = nullptr;
    QLineEdit *m_secondaryPathEdit = nullptr;
    QCheckBox *m_secondaryRecurse = nullptr;
    QSpinBox *m_sizeMinValue = nullptr;
    QComboBox *m_sizeMinUnit = nullptr;
    QSpinBox *m_sizeDiffValue = nullptr;
    QComboBox *m_sizeDiffUnit = nullptr;
    QPushButton *m_startButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTreeView *m_tree = nullptr;
    CheckBoxHeader *m_header = nullptr;
    QPushButton *m_linkButton = nullptr;

    QString m_primaryValidating;   // path awaiting on-connect validation
    QString m_secondaryValidating;
    QList<int> m_jobRows;          // LinkRunner job index -> model row
    bool m_lastSearching = false;  // last value emitted via searchRunningChanged
};
