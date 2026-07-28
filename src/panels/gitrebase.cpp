#include "gitrebase.h"
#include <QLabel>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QScrollBar>
#include <QJsonDocument>
#include <QJsonObject>

// ── RebaseCommitWidget ─────────────────────────────────────────────

RebaseCommitWidget::RebaseCommitWidget(const RebaseCommit &commit, QWidget *parent)
    : QWidget(parent)
    , m_commit(commit)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(8);

    m_actionCombo = new QComboBox(this);
    m_actionCombo->addItems({"pick", "squash", "edit", "reword", "fixup", "drop"});
    m_actionCombo->setCurrentText(commit.action);
    m_actionCombo->setFixedWidth(90);
    m_actionCombo->setToolTip(tr("Action for this commit: pick, squash, edit, reword, fixup, or drop"));
    connect(m_actionCombo, &QComboBox::currentTextChanged, this, &RebaseCommitWidget::actionChanged);
    layout->addWidget(m_actionCombo);

    m_hashLabel = new QLabel(commit.shortHash, this);
    m_hashLabel->setFixedWidth(90);
    m_hashLabel->setStyleSheet("color: palette(midlight); font-family: monospace;");
    layout->addWidget(m_hashLabel);

    m_messageLabel = new QLabel(commit.message, this);
    m_messageLabel->setWordWrap(false);
    layout->addWidget(m_messageLabel, 1);
}

QString RebaseCommitWidget::action() const
{
    return m_actionCombo->currentText();
}

void RebaseCommitWidget::setAction(const QString &action)
{
    int idx = m_actionCombo->findText(action);
    if (idx >= 0) m_actionCombo->setCurrentIndex(idx);
}

// ── GitRebaseWidget ─────────────────────────────────────────────────

GitRebaseWidget::GitRebaseWidget(QWidget *parent)
    : QWidget(parent)
    , m_process(new QProcess(this))
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Title
    QLabel *title = new QLabel(tr("Interactive Rebase"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    title->setFont(titleFont);
    mainLayout->addWidget(title);

    // Info label
    m_diffPreview = new QTextEdit(this);
    m_diffPreview->setReadOnly(true);
    m_diffPreview->setPlaceholderText(tr("Select a commit to see its diff preview..."));
    m_diffPreview->setMaximumHeight(120);

    // Splitter: commit list + diff preview
    QSplitter *splitter = new QSplitter(Qt::Vertical, this);

    // Commit list area
    QWidget *commitArea = new QWidget(this);
    QVBoxLayout *commitLayout = new QVBoxLayout(commitArea);
    commitLayout->setContentsMargins(0, 0, 0, 0);
    commitLayout->setSpacing(4);

    QLabel *infoLabel = new QLabel(tr("Reorder commits by selecting and using ▲▼ buttons. "
                                       "Change action via the dropdown."), this);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: palette(midlight); font-size: 11px;");
    commitLayout->addWidget(infoLabel);

    // Reorder buttons
    QHBoxLayout *reorderLayout = new QHBoxLayout();
    m_moveUpBtn = new QPushButton("▲", this);
    m_moveUpBtn->setFixedSize(28, 24);
    m_moveUpBtn->setToolTip(tr("Move selected commit up"));
    connect(m_moveUpBtn, &QPushButton::clicked, this, &GitRebaseWidget::onMoveUpClicked);
    reorderLayout->addWidget(m_moveUpBtn);

    m_moveDownBtn = new QPushButton("▼", this);
    m_moveDownBtn->setFixedSize(28, 24);
    m_moveDownBtn->setToolTip(tr("Move selected commit down"));
    connect(m_moveDownBtn, &QPushButton::clicked, this, &GitRebaseWidget::onMoveDownClicked);
    reorderLayout->addWidget(m_moveDownBtn);

    reorderLayout->addStretch();
    commitLayout->addLayout(reorderLayout);

    // Commit list
    m_commitList = new QListWidget(this);
    m_commitList->setAlternatingRowColors(true);
    m_commitList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_commitList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) showDiffForCommit(row);
    });
    commitLayout->addWidget(m_commitList, 1);

    splitter->addWidget(commitArea);
    splitter->addWidget(m_diffPreview);
    mainLayout->addWidget(splitter, 1);

    // Action buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_refreshBtn = new QPushButton(tr("⟳ Refresh"), this);
    m_refreshBtn->setToolTip(tr("Reload the rebase todo list"));
    connect(m_refreshBtn, &QPushButton::clicked, this, &GitRebaseWidget::onRefreshClicked);
    btnLayout->addWidget(m_refreshBtn);

    btnLayout->addStretch();

    m_continueBtn = new QPushButton(tr("Continue ▶"), this);
    m_continueBtn->setToolTip(tr("Continue with the modified rebase plan"));
    m_continueBtn->setStyleSheet("QPushButton { color: palette(highlight); font-weight: bold; }");
    btnLayout->addWidget(m_continueBtn);

    m_abortBtn = new QPushButton(tr("✕ Abort"), this);
    m_abortBtn->setToolTip(tr("Abort the rebase and return to the original state"));
    m_abortBtn->setStyleSheet("QPushButton { color: #cc4444; }");
    btnLayout->addWidget(m_abortBtn);

    mainLayout->addLayout(btnLayout);

    // Connections
    m_continueBtn->setEnabled(false);
    m_abortBtn->setEnabled(false);
    m_moveUpBtn->setEnabled(false);
    m_moveDownBtn->setEnabled(false);

    connect(m_continueBtn, &QPushButton::clicked, this, &GitRebaseWidget::onContinueClicked);
    connect(m_abortBtn, &QPushButton::clicked, this, &GitRebaseWidget::onAbortClicked);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &GitRebaseWidget::onProcessFinished);

    setStyleSheet(R"(
        GitRebaseWidget { background-color: palette(window); }
        QListWidget { border: 1px solid palette(mid); border-radius: 4px; }
        QListWidget::item { padding: 4px; border-bottom: 1px solid palette(midlight); }
        QListWidget::item:selected { background-color: palette(highlight); color: palette(highlighted-text); }
        QPushButton { padding: 4px 12px; border: 1px solid palette(mid); border-radius: 4px; }
        QPushButton:hover { background-color: palette(light); }
        QComboBox { padding: 2px 4px; border: 1px solid palette(mid); border-radius: 3px; }
    )");
}

bool GitRebaseWidget::isRebaseInProgress() const { return m_rebaseInProgress; }

void GitRebaseWidget::detectRebase()
{
    m_gitDir = QDir::currentPath();

    // Walk up to find .git directory
    QDir dir(m_gitDir);
    while (!dir.exists(".git") && dir.cdUp()) {}
    if (dir.exists(".git")) {
        m_gitDir = dir.absolutePath();
    }

    QString rebaseMergeDir = m_gitDir + "/.git/rebase-merge";
    m_rebaseInProgress = QDir(rebaseMergeDir).exists();

    if (m_rebaseInProgress) {
        loadTodoList();
    }

    updateUI();
}

void GitRebaseWidget::loadTodoList()
{
    QString todoFilePath = m_gitDir + "/.git/rebase-merge/git-rebase-todo";

    // Also try alternate location
    if (!QFile::exists(todoFilePath)) {
        todoFilePath = m_gitDir + "/.git/rebase-merge/git-rebase-todo.backup";
    }

    QFile todoFile(todoFilePath);
    if (todoFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray content = todoFile.readAll();
        todoFile.close();
        parseTodoList(content);
    } else {
        // If no todo file exists, run git to get the list
        m_process->start("git", {"-C", m_gitDir, "log", "--oneline", "--format=%h %an %ai %s",
                                 "HEAD..HEAD@{1}", "--reverse"});
    }
}

void GitRebaseWidget::parseTodoList(const QByteArray &output)
{
    m_commits.clear();
    m_originalCommits.clear();

    QString text = QString::fromUtf8(output);
    QStringList lines = text.split('\n');

    static QRegularExpression todoRe(R"(^(\S+)\s+([a-f0-9]+)\s+(.*)$)");
    static QRegularExpression logRe(R"(^([a-f0-9]+)\s+(\S+)\s+(\S+ \S+)\s+(.*)$)");

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#')) continue;

        QRegularExpressionMatch todoMatch = todoRe.match(trimmed);
        if (todoMatch.hasMatch()) {
            RebaseCommit commit;
            commit.action = todoMatch.captured(1);
            commit.hash = todoMatch.captured(2);
            commit.shortHash = commit.hash.left(7);
            commit.message = todoMatch.captured(3);
            m_commits.append(commit);
            continue;
        }

        QRegularExpressionMatch logMatch = logRe.match(trimmed);
        if (logMatch.hasMatch()) {
            RebaseCommit commit;
            commit.action = "pick";
            commit.hash = logMatch.captured(1);
            commit.shortHash = commit.hash.left(7);
            commit.author = logMatch.captured(2);
            commit.date = logMatch.captured(3);
            commit.message = logMatch.captured(4);
            m_commits.append(commit);
        }
    }

    m_originalCommits = m_commits;
    updateCommitList();
}

void GitRebaseWidget::updateCommitList()
{
    m_commitList->clear();
    m_commitWidgets.clear();

    for (int i = 0; i < m_commits.size(); ++i) {
        RebaseCommitWidget *widget = new RebaseCommitWidget(m_commits[i], this);
        connect(widget, &RebaseCommitWidget::actionChanged, this, [this, i](const QString &action) {
            if (i < m_commits.size()) {
                m_commits[i].action = action;
            }
        });

        QListWidgetItem *item = new QListWidgetItem(m_commitList);
        item->setSizeHint(widget->sizeHint());
        m_commitList->setItemWidget(item, widget);
        m_commitWidgets.append(widget);
    }

    m_moveUpBtn->setEnabled(m_commits.size() > 1);
    m_moveDownBtn->setEnabled(m_commits.size() > 1);
}

void GitRebaseWidget::saveTodoList()
{
    if (!m_rebaseInProgress || m_commits.isEmpty()) return;

    QString todoFilePath = m_gitDir + "/.git/rebase-merge/git-rebase-todo";

    // Generate the todo content
    QString todoContent;
    for (const RebaseCommit &c : m_commits) {
        todoContent += QString("%1 %2 %3\n").arg(c.action, c.hash, c.message);
    }

    QFile todoFile(todoFilePath);
    if (todoFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream stream(&todoFile);
        stream << todoContent;
        todoFile.close();
        qDebug() << "Updated rebase todo list with" << m_commits.size() << "commits";
    }

    // Also update the backup file
    QString backupPath = todoFilePath + ".backup";
    QFile backupFile(backupPath);
    if (backupFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream stream(&backupFile);
        stream << todoContent;
        backupFile.close();
    }
}

void GitRebaseWidget::updateUI()
{
    m_continueBtn->setEnabled(m_rebaseInProgress);
    m_abortBtn->setEnabled(m_rebaseInProgress);
    m_refreshBtn->setEnabled(m_rebaseInProgress);
    m_moveUpBtn->setEnabled(m_rebaseInProgress && m_commits.size() > 1);
    m_moveDownBtn->setEnabled(m_rebaseInProgress && m_commits.size() > 1);

    if (!m_rebaseInProgress) {
        m_diffPreview->clear();
        m_diffPreview->setPlaceholderText(tr("No rebase in progress. Start one with:\n"
                                              "  git rebase -i <branch>"));
    }
}

void GitRebaseWidget::onRefreshClicked()
{
    detectRebase();
}

void GitRebaseWidget::onContinueClicked()
{
    // Save the modified todo list first
    saveTodoList();

    // Continue the rebase
    m_process->start("git", {"-C", m_gitDir, "rebase", "--continue"});
}

void GitRebaseWidget::onAbortClicked()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Abort Rebase"),
        tr("Abort the current interactive rebase?\n"
           "All changes made during the rebase will be lost."),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_process->start("git", {"-C", m_gitDir, "rebase", "--abort"});
    }
}

void GitRebaseWidget::onMoveUpClicked()
{
    int row = m_commitList->currentRow();
    if (row <= 0) return;

    m_commits.swapItemsAt(row, row - 1);
    updateCommitList();
    m_commitList->setCurrentRow(row - 1);
}

void GitRebaseWidget::onMoveDownClicked()
{
    int row = m_commitList->currentRow();
    if (row < 0 || row >= m_commits.size() - 1) return;

    m_commits.swapItemsAt(row, row + 1);
    updateCommitList();
    m_commitList->setCurrentRow(row + 1);
}

void GitRebaseWidget::onCommitActionChanged(const QString &action)
{
    int row = m_commitList->currentRow();
    if (row >= 0 && row < m_commits.size()) {
        m_commits[row].action = action;
    }
}

void GitRebaseWidget::showDiffForCommit(int index)
{
    if (index < 0 || index >= m_commits.size()) {
        m_diffPreview->clear();
        return;
    }

    const RebaseCommit &commit = m_commits[index];

    // Show commit details in the diff preview
    QString diffHtml;
    diffHtml += QString("<h3>%1</h3>").arg(commit.message.toHtmlEscaped());
    diffHtml += QString("<p><b>Hash:</b> %1</p>").arg(commit.hash);
    if (!commit.author.isEmpty()) {
        diffHtml += QString("<p><b>Author:</b> %1</p>").arg(commit.author.toHtmlEscaped());
    }
    if (!commit.date.isEmpty()) {
        diffHtml += QString("<p><b>Date:</b> %1</p>").arg(commit.date.toHtmlEscaped());
    }
    diffHtml += QString("<p><b>Action:</b> %1</p>").arg(commit.action);

    // Get the actual diff from git if possible
    if (m_rebaseInProgress) {
        // Check if this commit has a parent to diff against
        QProcess diffProcess;
        diffProcess.setWorkingDirectory(m_gitDir);
        diffProcess.start("git", {"-C", m_gitDir, "show", "--stat", commit.hash, "--format=%B"});
        if (diffProcess.waitForFinished(5000)) {
            QString diffOutput = QString::fromUtf8(diffProcess.readAllStandardOutput());
            if (!diffOutput.isEmpty()) {
                diffHtml += QString("<pre style='font-size:10px;'>%1</pre>")
                                .arg(diffOutput.toHtmlEscaped().left(2000));
            }
        }
    }

    m_diffPreview->setHtml(diffHtml);
}

void GitRebaseWidget::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        // Re-check if rebase is still in progress
        QString rebaseMergeDir = m_gitDir + "/.git/rebase-merge";
        m_rebaseInProgress = QDir(rebaseMergeDir).exists();

        if (!m_rebaseInProgress) {
            emit rebaseContinued();
        }

        updateUI();

        if (m_rebaseInProgress) {
            loadTodoList(); // Reload for next step
        }
    } else {
        QString error = QString::fromUtf8(m_process->readAllStandardError());
        if (!error.isEmpty()) {
            emit errorOccurred(error);
            m_diffPreview->setHtml(QString("<p style='color:red;'><b>Error:</b> %1</p>")
                                        .arg(error.toHtmlEscaped()));
        }
    }
}
