#include <QTest>
#include <QSignalSpy>
#include <QTextCursor>
#include <QTextDocument>
#include "multi-cursor.h"
#include "test_multicursor.h"

void TestMultiCursor::testInitialState()
{
    MultiCursorManager mgr;
    QVERIFY(!mgr.hasCursors());
    QCOMPARE(mgr.cursorCount(), 0);
    QVERIFY(mgr.cursors().isEmpty());
}

void TestMultiCursor::testAddCursor()
{
    MultiCursorManager mgr;
    QTextDocument doc;
    QTextCursor c1(&doc);
    QTextCursor c2(&doc);
    mgr.addCursor(c1);
    mgr.addCursor(c2);
    QCOMPARE(mgr.cursorCount(), 2);
    QVERIFY(mgr.hasCursors());
}

void TestMultiCursor::testRemoveLastCursor()
{
    MultiCursorManager mgr;
    QTextDocument doc;
    mgr.addCursor(QTextCursor(&doc));
    mgr.addCursor(QTextCursor(&doc));
    mgr.removeLastCursor();
    QCOMPARE(mgr.cursorCount(), 1);
    mgr.removeLastCursor();
    QCOMPARE(mgr.cursorCount(), 0);
    QVERIFY(!mgr.hasCursors());
}

void TestMultiCursor::testRemoveLastWhenEmpty()
{
    MultiCursorManager mgr;
    mgr.removeLastCursor(); // no crash
    QCOMPARE(mgr.cursorCount(), 0);
}

void TestMultiCursor::testClear()
{
    MultiCursorManager mgr;
    QTextDocument doc;
    mgr.addCursor(QTextCursor(&doc));
    mgr.clear();
    QCOMPARE(mgr.cursorCount(), 0);
}

void TestMultiCursor::testClearWhenEmpty()
{
    MultiCursorManager mgr;
    mgr.clear(); // no crash
    QCOMPARE(mgr.cursorCount(), 0);
}

void TestMultiCursor::testSetCursors()
{
    MultiCursorManager mgr;
    QTextDocument doc;
    QList<QTextCursor> cursors;
    cursors.append(QTextCursor(&doc));
    cursors.append(QTextCursor(&doc));
    cursors.append(QTextCursor(&doc));
    mgr.setCursors(cursors);
    QCOMPARE(mgr.cursorCount(), 3);
    QCOMPARE(mgr.cursors().size(), 3);
}

void TestMultiCursor::testHasCursors()
{
    MultiCursorManager mgr;
    QVERIFY(!mgr.hasCursors());
    QTextDocument doc;
    mgr.addCursor(QTextCursor(&doc));
    QVERIFY(mgr.hasCursors());
}

void TestMultiCursor::testCursorCount()
{
    MultiCursorManager mgr;
    QTextDocument doc;
    for (int i = 0; i < 5; ++i)
        mgr.addCursor(QTextCursor(&doc));
    QCOMPARE(mgr.cursorCount(), 5);
}

void TestMultiCursor::testSignalsEmitted()
{
    MultiCursorManager mgr;
    QSignalSpy spy(&mgr, &MultiCursorManager::cursorsChanged);

    QTextDocument doc;
    mgr.addCursor(QTextCursor(&doc));
    QCOMPARE(spy.count(), 1);

    mgr.removeLastCursor();
    QCOMPARE(spy.count(), 2);

    mgr.addCursor(QTextCursor(&doc));
    mgr.setCursors({});
    QCOMPARE(spy.count(), 4);
}
