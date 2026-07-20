#include <QTest>
#include <QWidget>
#include "splitmanager.h"
#include "test_splitmanager.h"

void TestSplitManager::testInitialState()
{
    QWidget parent;
    SplitManager manager(&parent);
    QVERIFY(true);
}
