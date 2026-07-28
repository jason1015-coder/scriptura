#include "taskrunnerui.h"
#include <QLabel>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QTextStream>

TaskRunnerUI::TaskRunnerUI(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    QLabel *title = new QLabel(tr("Tasks"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    m_taskList = new QListWidget(this);
    layout->addWidget(m_taskList, 1);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_runBtn = new QPushButton(tr("Run"), this);
    m_refreshBtn = new QPushButton(tr("Refresh"), this);
    btnLayout->addWidget(m_runBtn);
    btnLayout->addWidget(m_refreshBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(m_runBtn, &QPushButton::clicked, this, &TaskRunnerUI::onRunClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &TaskRunnerUI::onRefreshClicked);
    connect(m_taskList, &QListWidget::itemDoubleClicked, this, &TaskRunnerUI::onItemDoubleClicked);

    setStyleSheet(R"(
        TaskRunnerUI { background-color: palette(window); }
        QListWidget { border: 1px solid palette(mid); border-radius: 4px; }
        QPushButton { padding: 4px 12px; border: 1px solid palette(mid); border-radius: 4px; }
        QPushButton:hover { background-color: palette(light); }
    )");
}

void TaskRunnerUI::detectTasks(const QString &projectPath)
{
    m_tasks.clear();
    detectPackageJsonTasks(projectPath);
    detectMakefileTasks(projectPath);
    detectCargoTasks(projectPath);
    populateList();
}

void TaskRunnerUI::detectPackageJsonTasks(const QString &projectPath)
{
    QFile file(projectPath + "/package.json");
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject scripts = doc["scripts"].toObject();

    for (auto it = scripts.constBegin(); it != scripts.constEnd(); ++it) {
        DetectedTask task;
        task.name = it.key();
        task.command = "npm run " + it.key();
        task.source = "package.json";
        task.description = it.value().toString();
        m_tasks.append(task);
    }
}

void TaskRunnerUI::detectMakefileTasks(const QString &projectPath)
{
    QFile file(projectPath + "/Makefile");
    if (!file.open(QIODevice::ReadOnly)) return;

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.endsWith(':') && !line.startsWith('.') && !line.startsWith('#')) {
            DetectedTask task;
            task.name = line.left(line.length() - 1).trimmed();
            task.command = "make " + task.name;
            task.source = "Makefile";
            m_tasks.append(task);
        }
    }
}

void TaskRunnerUI::detectCargoTasks(const QString &projectPath)
{
    QFile file(projectPath + "/Cargo.toml");
    if (!file.exists()) return;

    QStringList cargoTasks = {"build", "test", "run", "clippy", "fmt"};
    for (const QString &t : cargoTasks) {
        DetectedTask task;
        task.name = t;
        task.command = "cargo " + t;
        task.source = "Cargo.toml";
        m_tasks.append(task);
    }
}

void TaskRunnerUI::populateList()
{
    m_taskList->clear();
    for (const DetectedTask &task : m_tasks) {
        m_taskList->addItem(QString("[%1] %2").arg(task.source, task.name));
    }
}

void TaskRunnerUI::onRunClicked()
{
    int row = m_taskList->currentRow();
    if (row >= 0 && row < m_tasks.size()) {
        emit taskRun(m_tasks[row].command);
        emit taskSelected(m_tasks[row]);
    }
}

void TaskRunnerUI::onRefreshClicked()
{
    emit taskRun("refresh");
}

void TaskRunnerUI::onItemDoubleClicked(QListWidgetItem *item)
{
    Q_UNUSED(item);
    onRunClicked();
}
