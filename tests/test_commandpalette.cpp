#include <QTest>
#include "commandpalette.h"
#include "test_commandpalette.h"

void TestCommandPalette::testInitialState()
{
    CommandPalette palette;
    QVERIFY(!palette.isVisible());
}
