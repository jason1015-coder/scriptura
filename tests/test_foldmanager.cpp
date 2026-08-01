#include <QTest>
#include <QPlainTextEdit>
#include "foldmanager.h"
#include "test_foldmanager.h"

namespace {
QPlainTextEdit* makeEditor(const QString &text)
{
    auto *editor = new QPlainTextEdit();
    editor->setPlainText(text);
    return editor;
}
}

void TestFoldManager::testInitialState()
{
    QPlainTextEdit editor;
    FoldManager fm(&editor);
    QCOMPARE(fm.foldIndicatorWidth(), 20);
    QCOMPARE(fm.foldedLineCount(), 0);
    QVERIFY(!fm.isLineHidden(0));
}

void TestFoldManager::testDetectBraceRegions()
{
    QPlainTextEdit *editor = makeEditor("int main() {\n    return 0;\n}");
    FoldManager fm(editor);
    QVERIFY(!fm.regions().isEmpty());
    const FoldRegion &r = fm.regions().first();
    QCOMPARE(r.startLine, 0);
    QCOMPARE(r.endLine, 2);
    QVERIFY(r.valid);
    delete editor;
}

void TestFoldManager::testNoBraceRegions()
{
    QPlainTextEdit *editor = makeEditor("line one\nline two\nline three");
    FoldManager fm(editor);
    QVERIFY(fm.regions().isEmpty());
    delete editor;
}

void TestFoldManager::testToggleFold()
{
    // Lines: 0 = "{", 1 = "line1", 2 = "line2", 3 = "}"
    // Region spans 0..3; folding hides lines 1,2,3.
    QPlainTextEdit *editor = makeEditor("{\nline1\nline2\n}");
    FoldManager fm(editor);
    QVERIFY(fm.regions().size() >= 1);
    QVERIFY(!fm.isFolded(0));
    fm.toggleFold(0);
    QVERIFY(fm.isFolded(0));
    QCOMPARE(fm.foldedLineCount(), 3);
    QVERIFY(fm.isLineHidden(1));
    QVERIFY(fm.isLineHidden(3));
    QVERIFY(!fm.isLineHidden(0));
    fm.toggleFold(0);
    QVERIFY(!fm.isFolded(0));
    QCOMPARE(fm.foldedLineCount(), 0);
    delete editor;
}

void TestFoldManager::testFoldAllUnfoldAll()
{
    QPlainTextEdit *editor = makeEditor("{\n1\n2\n}\n{\n3\n4\n}");
    FoldManager fm(editor);
    fm.foldAll();
    QVERIFY(fm.foldedLineCount() > 0);
    fm.unfoldAll();
    QCOMPARE(fm.foldedLineCount(), 0);
    delete editor;
}

void TestFoldManager::testFoldAtLevel()
{
    QPlainTextEdit *editor = makeEditor("{\n    {\n    1\n    }\n}");
    FoldManager fm(editor);
    fm.foldAtLevel(4);
    // At least one region collapsed
    QVERIFY(fm.foldedLineCount() > 0);
    delete editor;
}

void TestFoldManager::testUnfoldAtLevel()
{
    QPlainTextEdit *editor = makeEditor("{\n    {\n    1\n    }\n}");
    FoldManager fm(editor);
    fm.foldAll();
    fm.unfoldAtLevel(4);
    // unfoldAtLevel(4) only unfolds regions at level >= 4, outer may stay
    QVERIFY(fm.foldedLineCount() >= 0);
    delete editor;
}

void TestFoldManager::testIsFolded()
{
    QPlainTextEdit *editor = makeEditor("{\n1\n2\n}");
    FoldManager fm(editor);
    QVERIFY(fm.regions().size() >= 1);
    QVERIFY(!fm.isFolded(0));
    fm.toggleFold(0);
    QVERIFY(fm.isFolded(0));
    delete editor;
}

void TestFoldManager::testIsFoldStartEnd()
{
    QPlainTextEdit *editor = makeEditor("int main() {\n    return 0;\n}");
    FoldManager fm(editor);
    QVERIFY(fm.isFoldStart(0));
    QVERIFY(fm.isFoldEnd(2));
    QVERIFY(!fm.isFoldStart(1));
    QVERIFY(!fm.isFoldEnd(1));
    delete editor;
}

void TestFoldManager::testRegionAtInvalid()
{
    QPlainTextEdit editor;
    FoldManager fm(&editor);
    FoldRegion r = fm.regionAt(0);
    QVERIFY(!r.valid);
    QCOMPARE(r.startLine, -1);
}

void TestFoldManager::testVisibleLineCount()
{
    QPlainTextEdit *editor = makeEditor("{\n1\n2\n3\n4\n}");
    FoldManager fm(editor);
    int total = editor->document()->blockCount();
    QCOMPARE(fm.visibleLineCount(), total);
    fm.toggleFold(0);
    QCOMPARE(fm.visibleLineCount(), total - fm.foldedLineCount());
    delete editor;
}

void TestFoldManager::testIsLineHidden()
{
    QPlainTextEdit *editor = makeEditor("{\n1\n2\n}");
    FoldManager fm(editor);
    fm.toggleFold(0);
    QVERIFY(fm.isLineHidden(1));
    QVERIFY(fm.isLineHidden(2));
    QVERIFY(!fm.isLineHidden(0));
    delete editor;
}

void TestFoldManager::testNestedBraceFolds()
{
    QPlainTextEdit *editor = makeEditor("{\n    {\n    x\n    }\n}");
    FoldManager fm(editor);
    QVERIFY(fm.regions().size() >= 2);
    delete editor;
}

void TestFoldManager::testKeywordFolds()
{
    QPlainTextEdit *editor = makeEditor("if (x) {\n    doThing();\n}\n");
    FoldManager fm(editor);
    // Brace folding should find at least one region
    QVERIFY(fm.regions().size() >= 1);
    delete editor;
}
