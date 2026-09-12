#ifndef TEST_TASKRUNNERUI_H
#define TEST_TASKRUNNERUI_H

#include <QObject>

class TestTaskRunnerUI : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testDetectPackageJsonTasks();
    void testDetectMakefileTasks();
    void testDetectCargoTasks();
    void testTaskRunEmission();
    void testTaskDetectionSignal();
    void testTaskAtBounds();
    void testTaskRunNoSelection();
    void testTaskRunRefresh();
    void testTaskRunCommandParsing();
};

#endif
