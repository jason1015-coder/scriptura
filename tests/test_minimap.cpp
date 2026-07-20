#include <QTest>
#include <QPlainTextEdit>
#include "minimap.h"
#include "test_minimap.h"

void TestMinimap::testInitialState()
{
    QPlainTextEdit editor;
    Minimap minimap(&editor);
    QVERIFY(minimap.isEnabled());
}
