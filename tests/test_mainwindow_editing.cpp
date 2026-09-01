#include <QTest>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QTextCursor>

#include "mainwindow.h"
#include "codeeditor.h"
#include "test_mainwindow_editing.h"

namespace {

QString makeTempProject(QTemporaryDir &dir, const QString &fileName = "sample.txt")
{
    QFile f(dir.filePath(fileName));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    QTextStream out(&f);
    out << "line one\nline two\nline three\n";
    f.close();
    return dir.filePath(fileName);
}

CodeEditor *activeEditor(MainWindow &win)
{
    return win.getCurrentCodeEditor();
}

// Focus the editor the way a user does (click equivalent), then pump the loop.
void focusEditor(MainWindow &win, CodeEditor *editor)
{
    editor->setFocus(Qt::MouseFocusReason);
    editor->viewport()->setFocus();
    QTest::qWait(20);
    QCoreApplication::processEvents();
    Q_UNUSED(win);
}

} // namespace

void TestMainWindowEditing::testEditorIsEditableAfterOpen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = makeTempProject(dir);
    QVERIFY(!filePath.isEmpty());

    MainWindow win(dir.path(), QStringList{filePath});
    win.show();
    QTest::qWait(50);
    QCoreApplication::processEvents();

    CodeEditor *editor = activeEditor(win);
    QVERIFY2(editor, "opening a file must produce a CodeEditor tab");
    QVERIFY2(editor->isEnabled(), "editor must be enabled");
    QVERIFY2(!editor->isReadOnly(), "editor must not be read-only");
    QVERIFY2(editor->toPlainText() == "line one\nline two\nline three\n",
             "file content must be loaded into the document");
}

void TestMainWindowEditing::testTypingInsertsText()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = makeTempProject(dir);
    QVERIFY(!filePath.isEmpty());

    MainWindow win(dir.path(), QStringList{filePath});
    win.show();
    QTest::qWait(50);
    QCoreApplication::processEvents();

    CodeEditor *editor = activeEditor(win);
    QVERIFY(editor);
    focusEditor(win, editor);

    // Place the caret at the end of the document (like clicking at the end).
    QTextCursor c = editor->textCursor();
    c.movePosition(QTextCursor::End);
    editor->setTextCursor(c);

    QTest::keyClicks(editor, "abc");
    QTest::qWait(20);
    QCoreApplication::processEvents();

    QCOMPARE(editor->toPlainText(), QStringLiteral("line one\nline two\nline three\nabc"));
    QCOMPARE(editor->textCursor().positionInBlock(),
             editor->textCursor().block().text().length());
}

void TestMainWindowEditing::testEnterInsertsNewline()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = makeTempProject(dir);
    QVERIFY(!filePath.isEmpty());

    MainWindow win(dir.path(), QStringList{filePath});
    win.show();
    QTest::qWait(50);
    QCoreApplication::processEvents();

    CodeEditor *editor = activeEditor(win);
    QVERIFY(editor);
    focusEditor(win, editor);

    QTextCursor c = editor->textCursor();
    c.movePosition(QTextCursor::End);
    editor->setTextCursor(c);

    // Enter must insert a newline (or smart-indent) — it must not be swallowed.
    QTest::keyClick(editor, Qt::Key_Return);
    QTest::keyClicks(editor, "x");
    QTest::qWait(20);
    QCoreApplication::processEvents();

    QVERIFY2(editor->toPlainText().contains("\n"), "Enter must insert a newline");
    QVERIFY2(editor->toPlainText().endsWith("x"),
             "typing after Enter must land in the new line");
}

void TestMainWindowEditing::testArrowKeysMoveCaret()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = makeTempProject(dir);
    QVERIFY(!filePath.isEmpty());

    MainWindow win(dir.path(), QStringList{filePath});
    win.show();
    QTest::qWait(50);
    QCoreApplication::processEvents();

    CodeEditor *editor = activeEditor(win);
    QVERIFY(editor);
    focusEditor(win, editor);

    QTextCursor c = editor->textCursor();
    c.movePosition(QTextCursor::Start);
    editor->setTextCursor(c);
    const int startPos = editor->textCursor().position();

    QTest::keyClick(editor, Qt::Key_Right);
    QTest::qWait(20);
    QCoreApplication::processEvents();

    QVERIFY2(editor->textCursor().position() > startPos,
             "Right arrow must move the caret");
}

void TestMainWindowEditing::testTabInsertsIndent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = makeTempProject(dir);
    QVERIFY(!filePath.isEmpty());

    MainWindow win(dir.path(), QStringList{filePath});
    win.show();
    QTest::qWait(50);
    QCoreApplication::processEvents();

    CodeEditor *editor = activeEditor(win);
    QVERIFY(editor);
    focusEditor(win, editor);

    QTextCursor c = editor->textCursor();
    c.movePosition(QTextCursor::End);
    editor->setTextCursor(c);
    const int startPos = editor->textCursor().position();

    // Send the Tab key directly via QApplication::sendEvent so the event
    // still flows through MainWindow's event filter (installed on the editor
    // in openFileInTab) — that's the integration path this test guards
    // (input must not be swallowed by the filter). We deliberately avoid
    // QTest::keyClick here: on the Windows offscreen platform, keyClick's
    // qWaitForWindowActive spins for the complex MainWindow and the test
    // is then killed by ctest's --timeout, failing the job.
    QKeyEvent tabPress(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
    QKeyEvent tabRelease(QEvent::KeyRelease, Qt::Key_Tab, Qt::NoModifier);
    QApplication::sendEvent(editor, &tabPress);
    QApplication::sendEvent(editor, &tabRelease);
    QTest::qWait(20);
    QCoreApplication::processEvents();

    QVERIFY2(editor->textCursor().position() > startPos,
             "Tab must move the caret (insert tab or indent), not be swallowed");
}

void TestMainWindowEditing::testCompletionPopupDoesNotBlockTyping()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = makeTempProject(dir);
    QVERIFY(!filePath.isEmpty());

    MainWindow win(dir.path(), QStringList{filePath});
    win.show();
    QTest::qWait(50);
    QCoreApplication::processEvents();

    CodeEditor *editor = activeEditor(win);
    QVERIFY(editor);
    focusEditor(win, editor);

    QTextCursor c = editor->textCursor();
    c.movePosition(QTextCursor::End);
    editor->setTextCursor(c);
    const int startPos = editor->textCursor().position();

    // Show the real completion popup through MainWindow's LSP result handler.
    QJsonArray items;
    QJsonObject item;
    item["label"] = "proposed";
    item["kind"] = "function";
    item["insertText"] = "proposed";
    items.append(item);
    // Fire the same signal the LSP adapter emits on a completion response;
    // MainWindow::onCompletionReceived (a plain private method, not a slot)
    // is connected to it and shows the popup.
    emit win.getLspClient()->completionReceived(items, 1);
    QTest::qWait(20);
    QCoreApplication::processEvents();

    // Typing with the popup visible must land in the document.
    QTest::keyClicks(editor, "x");
    QTest::qWait(20);
    QCoreApplication::processEvents();

    QVERIFY2(editor->textCursor().position() > startPos,
             "typing must insert a character even while the popup is visible");

    // And after typing (which hides the popup), Enter must insert a newline —
    // not be swallowed by a still-visible popup.
    QTest::keyClick(editor, Qt::Key_Return);
    QTest::qWait(20);
    QCoreApplication::processEvents();
    QVERIFY2(editor->toPlainText().contains("x\n"),
             "Enter after typing must insert a newline (popup released input)");
}
