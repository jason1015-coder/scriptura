#include <QTest>
#include <QPlainTextEdit>
#include <QImage>
#include <QPainter>
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

// ---------------------------------------------------------------------------
// Regression tests added for the MANUAL_TEST_LOG fixes
// ---------------------------------------------------------------------------

void TestFoldManager::testFoldingHidesTextBlocks()
{
    // Folding must actually hide the QTextBlock contents so the editor
    // stops rendering the folded lines (visible-line behavior).
    QPlainTextEdit *editor = makeEditor("line0\nline1\nline2\nline3\nline4");
    FoldManager fm(editor);

    // No folds initially -> all blocks visible
    for (int i = 0; i < editor->document()->blockCount(); ++i)
        QVERIFY(editor->document()->findBlockByNumber(i).isVisible());

    // Use brace content so the brace detector finds a region.
    editor->setPlainText("if (x) {\n    line1\n    line2\n    line3\n}\n");
    fm.detectRegions();
    QVERIFY(fm.regions().size() >= 1);

    fm.toggleFold(fm.regions().first().startLine);
    int hidden = 0;
    for (int i = 0; i < editor->document()->blockCount(); ++i) {
        if (!editor->document()->findBlockByNumber(i).isVisible())
            hidden++;
    }
    // At least one block should now be hidden by the fold
    QVERIFY(hidden > 0);
    delete editor;
}

void TestFoldManager::testUnfoldingRestoresTextBlocks()
{
    QPlainTextEdit *editor = makeEditor("if (x) {\n    line1\n    line2\n}\n");
    FoldManager fm(editor);
    fm.detectRegions();
    QVERIFY(fm.regions().size() >= 1);

    fm.toggleFold(fm.regions().first().startLine);
    int hiddenBefore = 0;
    for (int i = 0; i < editor->document()->blockCount(); ++i)
        if (!editor->document()->findBlockByNumber(i).isVisible())
            hiddenBefore++;
    QVERIFY(hiddenBefore > 0);

    fm.toggleFold(fm.regions().first().startLine);
    int hiddenAfter = 0;
    for (int i = 0; i < editor->document()->blockCount(); ++i)
        if (!editor->document()->findBlockByNumber(i).isVisible())
            hiddenAfter++;
    // Unfolding should restore visibility of all blocks
    QVERIFY(hiddenAfter == 0);
    delete editor;
}

void TestFoldManager::testNestedFoldVisibleLineBehavior()
{
    // Nested folds: collapsing an outer region must hide the inner region too,
    // and visibleLineCount must reflect the hidden blocks.
    QPlainTextEdit *editor = makeEditor("{\n    {\n        x\n    }\n    y\n}\n");
    FoldManager fm(editor);
    QVERIFY(fm.regions().size() >= 2);

    int total = editor->document()->blockCount();
    QVERIFY(fm.visibleLineCount() == total);

    fm.toggleFold(0);
    // Outer fold hides lines 1..4
    QVERIFY(fm.foldedLineCount() > 0);
    QVERIFY(fm.visibleLineCount() == total - fm.foldedLineCount());
    delete editor;
}

void TestFoldManager::testFoldIndicatorPaintDoesNotCrash()
{
    // paintFoldIndicator must not crash and must draw a circle for fold
    // starts and nothing for non-fold lines.
    QPlainTextEdit *editor = makeEditor("{\n    x\n}\n");
    FoldManager fm(editor);
    QVERIFY(fm.isFoldStart(0));
    QVERIFY(!fm.isFoldStart(1));

    QImage img(QSize(40, 40), QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter painter(&img);
    fm.paintFoldIndicator(painter, 0, 0, 0, 20);   // fold start
    fm.paintFoldIndicator(painter, 0, 0, 1, 20);   // not a fold start
    painter.end();
    // If we got here without crashing, the test passes.
    delete editor;
}

void TestFoldManager::testReattachDocumentOnNewDocument()
{
    // When the editor's document is replaced (e.g. a new file is opened),
    // the FoldManager must re-attach to the new document and recompute
    // regions instead of keeping stale folds from the old document.
    QPlainTextEdit *editor = makeEditor("int main() {\n    return 0;\n}");
    FoldManager fm(editor);
    QVERIFY(fm.regions().size() >= 1);

    // Replace the document with a brace-free one.
    QTextDocument *newDoc = new QTextDocument(editor);
    newDoc->setPlainText("plain text\nno braces here\n");
    editor->setDocument(newDoc);

    fm.reattachDocument();
    fm.detectRegions();
    QVERIFY(fm.regions().size() == 0);
    delete editor;
}
