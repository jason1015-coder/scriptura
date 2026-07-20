#ifndef TEST_CRASHHANDLER_H
#define TEST_CRASHHANDLER_H

#include <QObject>

class TestCrashHandler : public QObject
{
    Q_OBJECT
private slots:
    void testInstall();
    void testDumpPath();
};

#endif
