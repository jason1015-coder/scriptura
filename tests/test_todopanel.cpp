#include <QTest>
#include "todopanel.h"
#include "test_todopanel.h"

void TestTodoPanel::testInitialState()
{
    TodoPanel panel;
    QVERIFY(panel.isEnabled());
    QVERIFY(!panel.isVisible());
}
