#ifndef GITSTASH_H
#define GITSTASH_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QProcess>
#include <QMap>

/**
 * Represents a single Git stash entry.
 */
struct StashEntry {
    int index;
    QString branch;
    QString message;
    QString hash;
};

/**
 * Git stash management panel.
 * Features:
 * - View list of all stash entries
 * - Apply stash (keep in stash list)
 * - Pop stash (apply and remove)
 * - Drop stash (delete without applying)
 * - Create stash with custom message
 * - View stash diff before applying
 */
class GitStashWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GitStashWidget(QWidget *parent = nullptr);

    // Refresh the stash list
    void refresh();

    // Get current stash entries
    QList<StashEntry> stashEntries() const { return m_entries; }

signals:
    void stashApplied(int index);
    void stashPopped(int index);
    void stashDropped(int index);
    void stashCreated(const QString &message);
    void errorOccurred(const QString &error);

private slots:
    void onRefreshClicked();
    void onApplyClicked();
    void onPopClicked();
    void onDropClicked();
    void onCreateClicked();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void parseStashOutput(const QByteArray &output);
    void showStashDiff(int index);

    QListWidget *m_stashList;
    QPushButton *m_refreshBtn;
    QPushButton *m_applyBtn;
    QPushButton *m_popBtn;
    QPushButton *m_dropBtn;
    QPushButton *m_createBtn;
    QProcess *m_process;
    QList<StashEntry> m_entries;
};

#endif // GITSTASH_H
