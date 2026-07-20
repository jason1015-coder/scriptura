#include <QTest>
#include "terminalpanel.h"
#include "test_terminalpanel.h"

void TestTerminalPanel::testInitialState()
{
    TerminalPanel panel;
    QVERIFY(!panel.isRunning());
    QVERIFY(panel.workingDirectory().isEmpty());
}

void TestTerminalPanel::testWorkingDirectory()
{
    TerminalPanel panel;
    QString dir = "/tmp";
    panel.setWorkingDirectory(dir);
    QCOMPARE(panel.workingDirectory(), dir);
}
