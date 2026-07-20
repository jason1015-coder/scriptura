#include <QTest>
#include <QTemporaryDir>
#include "projectsearch.h"
#include "test_projectsearch.h"

void TestProjectSearch::testInitialState()
{
    ProjectSearchPanel panel;
    QVERIFY(panel.isEnabled());
    QVERIFY(panel.currentRootPath().isEmpty());
}

void TestProjectSearch::testSetRootPath()
{
    ProjectSearchPanel panel;
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    panel.setRootPath(tempDir.path());
    QCOMPARE(panel.currentRootPath(), tempDir.path());
}

void TestProjectSearch::testClearResults()
{
    ProjectSearchPanel panel;
    panel.clearResults(); // should not crash
    QVERIFY(true);
}
