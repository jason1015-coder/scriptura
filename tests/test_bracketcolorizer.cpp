#include <QTest>
#include <QPlainTextEdit>
#include "bracketcolorizer.h"
#include "test_bracketcolorizer.h"

void TestBracketColorizer::testInitialState()
{
    QPlainTextEdit editor;
    BracketColorizer bc(&editor);
    QVERIFY(bc.isEnabled());
    QVERIFY(bc.extraSelections().isEmpty());
}

void TestBracketColorizer::testEmptyDocument()
{
    QPlainTextEdit editor;
    BracketColorizer bc(&editor);
    bc.updateColors();
    QVERIFY(bc.extraSelections().isEmpty());
    QCOMPARE(bc.depthAt(0), -1);
}

void TestBracketColorizer::testSimplePair()
{
    QPlainTextEdit editor;
    editor.setPlainText("(hello)");
    BracketColorizer bc(&editor);
    bc.updateColors();
    QCOMPARE(bc.depthAt(0), 0);
    QCOMPARE(bc.depthAt(6), 0);
    BracketPair pair = bc.pairAt(0);
    QCOMPARE(pair.openPos, 0);
    QCOMPARE(pair.closePos, 6);
    QCOMPARE(pair.depth, 0);
}

void TestBracketColorizer::testNestedPairs()
{
    QPlainTextEdit editor;
    editor.setPlainText("([{x}])");
    BracketColorizer bc(&editor);
    bc.updateColors();
    QCOMPARE(bc.depthAt(0), 0);
    QCOMPARE(bc.depthAt(1), 1);
    QCOMPARE(bc.depthAt(2), 2);
    QCOMPARE(bc.depthAt(4), 2);
    QCOMPARE(bc.depthAt(5), 1);
    QCOMPARE(bc.depthAt(6), 0);
}

void TestBracketColorizer::testMismatchedBrackets()
{
    QPlainTextEdit editor;
    editor.setPlainText("(]");
    BracketColorizer bc(&editor);
    bc.updateColors();
    // Mismatched pairs are not recorded
    QCOMPARE(bc.depthAt(0), -1);
    QCOMPARE(bc.depthAt(1), -1);
}

void TestBracketColorizer::testStringsIgnored()
{
    QPlainTextEdit editor;
    editor.setPlainText("\"( )\"");
    BracketColorizer bc(&editor);
    bc.updateColors();
    QCOMPARE(bc.depthAt(2), -1);
    QCOMPARE(bc.depthAt(4), -1);
}

void TestBracketColorizer::testLineCommentsIgnored()
{
    QPlainTextEdit editor;
    editor.setPlainText("// ( )");
    BracketColorizer bc(&editor);
    bc.updateColors();
    QCOMPARE(bc.depthAt(3), -1);
    QCOMPARE(bc.depthAt(5), -1);
}

void TestBracketColorizer::testDepthAt()
{
    QPlainTextEdit editor;
    editor.setPlainText("((x))");
    BracketColorizer bc(&editor);
    bc.updateColors();
    QCOMPARE(bc.depthAt(0), 0);
    QCOMPARE(bc.depthAt(1), 1);
    QCOMPARE(bc.depthAt(2), -1); // 'x' is not a bracket
    QCOMPARE(bc.depthAt(3), 1);
    QCOMPARE(bc.depthAt(4), 0);
}

void TestBracketColorizer::testPairAtInvalid()
{
    QPlainTextEdit editor;
    editor.setPlainText("abc");
    BracketColorizer bc(&editor);
    bc.updateColors();
    BracketPair pair = bc.pairAt(0);
    QCOMPARE(pair.openPos, -1);
    QCOMPARE(pair.closePos, -1);
    QCOMPARE(pair.depth, -1);
}

void TestBracketColorizer::testSetEnabledOff()
{
    QPlainTextEdit editor;
    editor.setPlainText("(x)");
    BracketColorizer bc(&editor);
    bc.updateColors();
    QVERIFY(!bc.extraSelections().isEmpty());

    bc.setEnabled(false);
    QVERIFY(!bc.isEnabled());
    QVERIFY(bc.extraSelections().isEmpty());
}

void TestBracketColorizer::testSetEnabledOn()
{
    QPlainTextEdit editor;
    editor.setPlainText("(x)");
    BracketColorizer bc(&editor);
    bc.setEnabled(false);
    bc.setEnabled(true);
    QVERIFY(bc.isEnabled());
    QVERIFY(!bc.extraSelections().isEmpty());
}

void TestBracketColorizer::testCustomColors()
{
    QPlainTextEdit editor;
    editor.setPlainText("()");
    BracketColorizer bc(&editor);
    QList<QColor> colors = { QColor(Qt::red), QColor(Qt::blue) };
    bc.setBracketColors(colors);
    bc.updateColors();
    QVERIFY(!bc.extraSelections().isEmpty());
}

void TestBracketColorizer::testSetBracketColorsWhenDisabled()
{
    QPlainTextEdit editor;
    editor.setPlainText("()");
    BracketColorizer bc(&editor);
    bc.setEnabled(false);
    QList<QColor> colors = { QColor(Qt::red) };
    bc.setBracketColors(colors); // should not crash when disabled
    QVERIFY(bc.extraSelections().isEmpty());
}

void TestBracketColorizer::testClearColors()
{
    QPlainTextEdit editor;
    editor.setPlainText("()");
    BracketColorizer bc(&editor);
    bc.updateColors();
    QVERIFY(!bc.extraSelections().isEmpty());
    bc.clearColors();
    QVERIFY(bc.extraSelections().isEmpty());
}

void TestBracketColorizer::testExtraSelectionsCount()
{
    QPlainTextEdit editor;
    editor.setPlainText("(a) (b)");
    BracketColorizer bc(&editor);
    bc.updateColors();
    // 2 pairs => 4 selections (open + close each)
    QCOMPARE(bc.extraSelections().size(), 4);
}

void TestBracketColorizer::testRepeatedBracketDepth()
{
    QPlainTextEdit editor;
    editor.setPlainText("{[()[()]]}");
    BracketColorizer bc(&editor);
    bc.updateColors();
    // Trace: {[ ( ) [ ( ) ] ] }
    QCOMPARE(bc.depthAt(0), 0);   // {  matches }  at depth 0
    QCOMPARE(bc.depthAt(1), 1);   // [  matches ]  at depth 1
    QCOMPARE(bc.depthAt(2), 2);   // (  matches )  at depth 2
    QCOMPARE(bc.depthAt(3), 2);   // )  pairs with ( at depth 2
    QCOMPARE(bc.depthAt(4), 2);   // [  matches ]  at depth 2
    QCOMPARE(bc.depthAt(5), 3);   // (  matches )  at depth 3
    QCOMPARE(bc.depthAt(6), 3);   // )  pairs with ( at depth 3
    QCOMPARE(bc.depthAt(7), 2);   // ]  pairs with [ at depth 2
    QCOMPARE(bc.depthAt(8), 1);   // ]  pairs with [ at depth 1
    QCOMPARE(bc.depthAt(9), 0);   // }  pairs with { at depth 0
}
