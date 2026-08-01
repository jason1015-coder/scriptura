#ifndef TEST_MULTICURSOR_H
#define TEST_MULTICURSOR_H

#include <QObject>

class TestMultiCursor : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testAddCursor();
    void testRemoveLastCursor();
    void testRemoveLastWhenEmpty();
    void testClear();
    void testClearWhenEmpty();
    void testSetCursors();
    void testHasCursors();
    void testCursorCount();
    void testSignalsEmitted();
};

#endif // TEST_MULTICURSOR_H
