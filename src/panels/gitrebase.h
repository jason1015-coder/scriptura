#ifndef GITREBASE_H
#define GITREBASE_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProcess>
#include <QComboBox>
#include <QTextEdit>
#include <QLabel>
#include <QSplitter>

struct RebaseCommit {
    QString hash;
    QString shortHash;
    QString author;
    QString date;
    QString message;
    QString action; // pick, squash, edit, reword, drop, fixup
};

class RebaseCommitWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RebaseCommitWidget(const RebaseCommit &commit, QWidget *parent = nullptr);
    QString action() const;
    void setAction(const QString &action);
    RebaseCommit commit() const { return m_commit; }
    void setCommit(const RebaseCommit &commit) { m_commit = commit; }

signals:
    void actionChanged(const QString &action);

private:
    QComboBox *m_actionCombo;
    QLabel *m_hashLabel;
    QLabel *m_messageLabel;
    RebaseCommit m_commit;
};

class GitRebaseWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GitRebaseWidget(QWidget *parent = nullptr);
    bool isRebaseInProgress() const;
    void detectRebase();

signals:
    void rebaseContinued();
    void rebaseAborted();
    void errorOccurred(const QString &error);

private slots:
    void onContinueClicked();
    void onAbortClicked();
    void onRefreshClicked();
    void onMoveUpClicked();
    void onMoveDownClicked();
    void onProcessFinished(int exitCode, QProcess::ExitStatus);
    void onCommitActionChanged(const QString &action);

private:
    void loadTodoList();
    void parseTodoList(const QByteArray &output);
    void saveTodoList();
    void updateCommitList();
    void updateUI();
    void showDiffForCommit(int index);

    // Commit list
    QListWidget *m_commitList;
    QPushButton *m_moveUpBtn;
    QPushButton *m_moveDownBtn;

    // Action buttons
    QPushButton *m_continueBtn;
    QPushButton *m_abortBtn;
    QPushButton *m_refreshBtn;

    // Diff preview
    QTextEdit *m_diffPreview;

    // Process
    QProcess *m_process;

    // Data
    QList<RebaseCommit> m_commits;
    QList<RebaseCommit> m_originalCommits;
    QList<RebaseCommitWidget*> m_commitWidgets;
    bool m_rebaseInProgress = false;
    QString m_gitDir;
};

#endif // GITREBASE_H
