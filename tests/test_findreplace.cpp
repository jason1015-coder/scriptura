#include <QTest>
#include "findreplace.h"
#include "test_findreplace.h"

void TestFindReplace::testCreateAndHide()
{
    FindReplaceBar bar;
    QVERIFY(!bar.isVisible());
    QVERIFY(bar.isEnabled());
}
