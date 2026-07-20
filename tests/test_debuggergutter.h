#ifndef TEST_DEBUGGERGUTTER_H
#define TEST_DEBUGGERGUTTER_H

#include <QObject>

class TestDebuggerGutter : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testSetBreakpoints();
    void testToggleBreakpoint();
    void testClearBreakpoints();
};

#endif
