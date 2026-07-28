#include "gitstash.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QDir>
#include <QDateTime>

GitStashWidget::GitStashWidget(QWidget *parent)
    : QWidget(parent)
    , m_process(new QProcess(this))
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    // Header
    QLabel *title = new QLabel(tr("Git Stashes"), this);
    title->setObjectName("stashTitle");
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    // Stash list
    m_stashList = new QListWidget(this);
    m_stashList->setObjectName("stashList");
    layout->addWidget(m_stashList, 1);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_refreshBtn = new QPushButton(tr("Refresh"), this);
    m_applyBtn = new QPushButton(tr("Apply"), this);
    m_popBtn = new QPushButton(tr("Pop"), this);
    m_dropBtn = new QPushButton(tr("Drop"), this);
    m_createBtn = new QPushButton(tr("New Stash"), this);

    btnLayout->addWidget(m_refreshBtn);
    btnLayout->addWidget(m_applyBtn);
    btnLayout->addWidget(m_popBtn);
    btnLayout->addWidget(m_dropBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_createBtn);
    layout->addLayout(btnLayout);

    // Connections
    connect(m_refreshBtn, &QPushButton::clicked, this, &GitStashWidget::onRefreshClicked);
    connect(m_applyBtn, &QPushButton::clicked, this, &GitStashWidget::onApplyClicked);
    connect(m_popBtn, &QPushButton::clicked, this, &GitStashWidget::onPopClicked);
    connect(m_dropBtn, &QPushButton::clicked, this, &GitStashWidget::onDropClicked);
    connect(m_createBtn, &QPushButton::clicked, this, &GitStashWidget::onCreateClicked);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &GitStashWidget::onProcessFinished);

    // Styling
    setStyleSheet(R"(
        GitStashWidget { background-color: palette(window); }
        QLabel#stashTitle { color: palette(text); font-size: 12px; padding: 4px; }
        QListWidget#stashList { border: 1px solid palette(mid); border-radius: 4px; }
        QPushButton { padding: 4px 12px; border: 1px solid palette(mid); border-radius: 4px; }
        QPushButton:hover { background-color: palette(light); }
    )");
}

void GitStashWidget::refresh()
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }

    m_entries.clear();
    m_stashList->clear();

    m_process->start("git", {"stash", "list", "--format=%gd|%gs|%H"});
}

void GitStashWidget::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        QByteArray output = m_process->readAllStandardOutput();
        parseStashOutput(output);
    } else {
        QByteArray error = m_process->readAllStandardError();
        emit errorOccurred(QString::fromUtf8(error));
    }
}

void GitStashWidget::parseStashOutput(const QByteArray &output)
{
    QString text = QString::fromUtf8(output);
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QStringList parts = line.split('|');
        if (parts.size() >= 2) {
            StashEntry entry;
            entry.index = m_entries.size();
            entry.branch = parts[0].trimmed();
            entry.message = parts[1].trimmed();
            if (parts.size() >= 3) entry.hash = parts[2].trimmed();

            m_entries.append(entry);
            m_stashList->addItem(QString("stash@{%1}: %2 (%3)")
                .arg(entry.index)
                .arg(entry.message)
                .arg(entry.branch));
        }
    }
}

void GitStashWidget::onRefreshClicked()
{
    refresh();
}

void GitStashWidget::onApplyClicked()
{
    int row = m_stashList->currentRow();
    if (row < 0 || row >= m_entries.size()) return;

    m_process->start("git", {"stash", "apply", QString::number(row)});
    connect(m_process, &QProcess::finished, this, [this, row](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            emit stashApplied(row);
            refresh();
        }
    });
}

void GitStashWidget::onPopClicked()
{
    int row = m_stashList->currentRow();
    if (row < 0 || row >= m_entries.size()) return;

    m_process->start("git", {"stash", "pop", QString::number(row)});
    connect(m_process, &QProcess::finished, this, [this, row](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            emit stashPopped(row);
            refresh();
        }
    });
}

void GitStashWidget::onDropClicked()
{
    int row = m_stashList->currentRow();
    if (row < 0 || row >= m_entries.size()) return;

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Drop Stash"),
        tr("Are you sure you want to drop stash@{%1}?").arg(row),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        m_process->start("git", {"stash", "drop", QString::number(row)});
        connect(m_process, &QProcess::finished, this, [this, row](int exitCode, QProcess::ExitStatus) {
            if (exitCode == 0) {
                emit stashDropped(row);
                refresh();
            }
        });
    }
}

void GitStashWidget::onCreateClicked()
{
    bool ok;
    QString message = QInputDialog::getText(
        this, tr("Create Stash"),
        tr("Stash message (optional):"),
        QLineEdit::Normal,
        QString(),
        &ok
    );

    if (ok) {
        QStringList args = {"stash", "push"};
        if (!message.isEmpty()) {
            args << "-m" << message;
        }
        m_process->start("git", args);
        connect(m_process, &QProcess::finished, this, [this, message](int exitCode, QProcess::ExitStatus) {
            if (exitCode == 0) {
                emit stashCreated(message);
                refresh();
            }
        });
    }
}

void GitStashWidget::showStashDiff(int index)
{
    Q_UNUSED(index);
    // TODO: Show diff for selected stash entry
}
