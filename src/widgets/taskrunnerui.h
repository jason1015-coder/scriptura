#ifndef TASKRUNNERUI_H
#define TASKRUNNERUI_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMap>

struct DetectedTask {
    QString name;
    QString command;
    QString source; // package.json, Makefile, etc.
    QString description;
};

class TaskRunnerUI : public QWidget
{
    Q_OBJECT
public:
    explicit TaskRunnerUI(QWidget *parent = nullptr);

    void detectTasks(const QString &projectPath);
    QList<DetectedTask> detectedTasks() const { return m_tasks; }

signals:
    void taskSelected(const DetectedTask &task);
    void taskRun(const QString &command);

private slots:
    void onRunClicked();
    void onRefreshClicked();
    void onItemDoubleClicked(QListWidgetItem *item);

private:
    void detectPackageJsonTasks(const QString &projectPath);
    void detectMakefileTasks(const QString &projectPath);
    void detectCargoTasks(const QString &projectPath);
    void populateList();

    QListWidget *m_taskList;
    QPushButton *m_runBtn;
    QPushButton *m_refreshBtn;
    QList<DetectedTask> m_tasks;
};

#endif // TASKRUNNERUI_H
