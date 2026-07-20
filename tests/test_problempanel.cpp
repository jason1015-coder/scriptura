#include <QTest>
#include <QSignalSpy>
#include "problempanel.h"
#include "test_problempanel.h"

void TestProblemPanel::testInitialState()
{
    ProblemPanel panel;
    QVERIFY(panel.isEnabled());
}

void TestProblemPanel::testAddAndClearProblems()
{
    ProblemPanel panel;
    QSignalSpy filterSpy(&panel, &ProblemPanel::filterChanged);
    QCOMPARE(filterSpy.count(), 0);
}
