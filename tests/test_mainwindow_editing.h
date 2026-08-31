#ifndef TEST_MAINWINDOW_EDITING_H
#define TEST_MAINWINDOW_EDITING_H

#include <QObject>

// Integration tests that drive the REAL MainWindow (not a standalone editor):
// open a file through openFileInTab(), then send real key events through
// MainWindow's event filter into the editor, verifying that typing, Enter,
// arrows and the completion popup never dead-end. This is the path the app
// actually uses; unit tests that send QKeyEvent directly to a bare CodeEditor
// bypass the focus/event-filter layer where input can be swallowed.
class TestMainWindowEditing : public QObject
{
    Q_OBJECT
private slots:
    void testEditorIsEditableAfterOpen();
    void testTypingInsertsText();
    void testEnterInsertsNewline();
    void testArrowKeysMoveCaret();
    void testTabInsertsIndent();
    void testCompletionPopupDoesNotBlockTyping();
};

#endif // TEST_MAINWINDOW_EDITING_H
