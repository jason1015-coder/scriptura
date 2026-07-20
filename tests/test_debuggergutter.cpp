#include <QTest>
#include "codeeditor.h"
#include "debuggergutter.h"
#include "test_debuggergutter.h"

void TestDebuggerGutter::testInitialState()
{
    CodeEditor editor;
    DebuggerGutter gutter(&editor);
    QVERIFY(gutter.breakpoints().isEmpty());
}

void TestDebuggerGutter::testSetBreakpoints()
{
    CodeEditor editor;
    DebuggerGutter gutter(&editor);
    QSet<int> bps = {1, 5, 10, 15};
    gutter.setBreakpoints(bps);
    QCOMPARE(gutter.breakpoints(), bps);
}

void TestDebuggerGutter::testToggleBreakpoint()
{
    CodeEditor editor;
    DebuggerGutter gutter(&editor);
    gutter.toggleBreakpoint(3);
    QVERIFY(gutter.breakpoints().contains(3));
    gutter.toggleBreakpoint(3); // toggle off
    QVERIFY(!gutter.breakpoints().contains(3));
}

void TestDebuggerGutter::testClearBreakpoints()
{
    CodeEditor editor;
    DebuggerGutter gutter(&editor);
    gutter.toggleBreakpoint(1);
    gutter.toggleBreakpoint(2);
    QVERIFY(!gutter.breakpoints().isEmpty());
    gutter.clearBreakpoints();
    QVERIFY(gutter.breakpoints().isEmpty());
}
