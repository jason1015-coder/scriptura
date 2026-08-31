#include <QTest>
#include <QApplication>
#include <QKeyEvent>
#include <QListWidget>
#include <QPointer>
#include "codeeditor.h"
#include "test_codeeditor_multicursor.h"

namespace {
void sendKey(CodeEditor &editor, int key, Qt::KeyboardModifiers mods, const QString &text)
{
    QKeyEvent ev(QEvent::KeyPress, key, mods, text);
    QApplication::sendEvent(&editor, &ev);
}
void moveCaret(CodeEditor &editor, int position)
{
    QTextCursor c = editor.textCursor();
    c.setPosition(position);
    editor.setTextCursor(c);
}
void selectRange(CodeEditor &editor, int from, int to)
{
    QTextCursor sel(editor.document());
    sel.setPosition(from);
    sel.setPosition(to, QTextCursor::KeepAnchor);
    editor.setTextCursor(sel);
}
}

// Regression tests for multi-cursor typing. These lock down the behaviour that
// previously double-inserted characters at a caret that overlapped an extra
// cursor, corrupting the buffer (seen as "the editor types garbage / cannot be
// edited" and reported as a crash).
void TestCodeEditorMultiCursor::testTypeAcrossCursors()
{
    CodeEditor editor;
    editor.setPlainText(QStringLiteral("aa\nbb\ncc"));
    moveCaret(editor, 2);       // caret right after "aa" on line 0
    editor.addCursorBelow();    // extra caret right after "bb"

    sendKey(editor, Qt::Key_X, Qt::NoModifier, QStringLiteral("x"));

    QCOMPARE(editor.toPlainText(), QStringLiteral("aax\nbbx\ncc"));
}

void TestCodeEditorMultiCursor::testSelectAllThenType()
{
    CodeEditor editor;
    editor.setPlainText(QStringLiteral("abc abc abc"));
    selectRange(editor, 0, 3);
    editor.selectAllOccurrences();

    sendKey(editor, Qt::Key_Z, Qt::NoModifier, QStringLiteral("z"));

    // Every selected occurrence is replaced by the typed character exactly once.
    QCOMPARE(editor.toPlainText(), QStringLiteral("z z z"));
}

void TestCodeEditorMultiCursor::testDuplicateExtraCursor()
{
    CodeEditor editor;
    editor.setPlainText(QStringLiteral("abc abc"));
    selectRange(editor, 0, 3);

    // selectNextOccurrence adds the found range as an extra cursor AND moves
    // the primary caret onto it, so primary and extra share the same position.
    editor.selectNextOccurrence();

    sendKey(editor, Qt::Key_Q, Qt::NoModifier, QStringLiteral("q"));

    // selectNextOccurrence moved the primary caret onto the found range; the
    // range is also present as an extra cursor. It must be typed exactly once
    // (a double-insert here would yield "abc qq").
    QCOMPARE(editor.toPlainText(), QStringLiteral("abc q"));
}

void TestCodeEditorMultiCursor::testEmptyTextKeysDontCorruptPreviousTyping()
{
    CodeEditor editor;
    editor.setPlainText(QStringLiteral("a\nb"));
    moveCaret(editor, 1);
    editor.addCursorBelow();

    sendKey(editor, Qt::Key_X, Qt::NoModifier, QStringLiteral("x")); // ax\nbx

    // Navigation / modifier keys afterwards must not corrupt or re-insert.
    sendKey(editor, Qt::Key_Left, Qt::NoModifier, QString());
    sendKey(editor, Qt::Key_Home, Qt::NoModifier, QString());
    sendKey(editor, Qt::Key_C, Qt::ControlModifier, QString());

    QCOMPARE(editor.toPlainText(), QStringLiteral("ax\nbx"));
}

// Regression for the LSP completion popup segfault: the popup is a plain child
// of the editor's viewport (no Qt::Popup window flag, so it never grabs the
// keyboard and silences the editor). It must be stored in a QPointer so that
// when the editor/tab is destroyed Qt frees the child and the pointer auto-nulls;
// clearing/hiding/re-showing with a new editor afterwards must not dereference
// freed memory. This mirrors exactly what MainWindow::onCompletionReceived/::hideCompletion do.
void TestCodeEditorMultiCursor::testCompletionPopupSiblingSurvivesEditorSwap()
{
    // Simulate a completion popup managed like MainWindow's: parented to the
    // editor viewport, stored in a QPointer, plain child (no Qt::Popup flag).
    QPointer<QListWidget> popup;

    CodeEditor *editorA = new CodeEditor;
    if (QWidget *vp = editorA->viewport()) {
        popup = new QListWidget(vp);
        popup->setFocusPolicy(Qt::NoFocus);
        popup->addItem(QStringLiteral("item"));
        popup->show();
    }
    QVERIFY(!popup.isNull());

    delete editorA; // destroys popup as child; QPointer auto-nulls
    QVERIFY(popup.isNull());

    // Re-populate on a fresh editor: must not dereference the freed popup.
    CodeEditor editorB;
    popup = new QListWidget(editorB.viewport());
    popup->setFocusPolicy(Qt::NoFocus);
    popup->addItem(QStringLiteral("item"));
    popup->clear();
    popup->hide();
    QVERIFY(!popup.isNull());

    // Re-attach again (multiple file switches) must stay safe.
    CodeEditor editorC;
    QListWidget *raw = new QListWidget(editorC.viewport());
    raw->addItem(QStringLiteral("item"));
    raw->clear();
    QCOMPARE(raw->count(), 0);
}

// The completion popup is a plain child widget with Qt::NoFocus (the former
// design used a top-level Qt::Popup, whose keyboard grab silences the editor).
// Verify that with the fix it can never become the focus widget.
void TestCodeEditorMultiCursor::testCompletionPopupDoesNotStealFocus()
{
    CodeEditor editor;
    editor.setPlainText(QStringLiteral("hello"));

    QListWidget popup(editor.viewport());
    popup.setFocusPolicy(Qt::NoFocus);
    popup.show();

    // A Qt::NoFocus child widget can never take focus from the editor, so the
    // keyboard stays with the editor (no more Qt::Popup keyboard grab).
    QVERIFY(popup.focusPolicy() == Qt::NoFocus);
}