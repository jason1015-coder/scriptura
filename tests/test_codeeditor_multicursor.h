#ifndef TEST_CODEEDITOR_MULTICURSOR_H
#define TEST_CODEEDITOR_MULTICURSOR_H

#include <QObject>

class TestCodeEditorMultiCursor : public QObject
{
    Q_OBJECT
private slots:
    void testTypeAcrossCursors();
    void testSelectAllThenType();
    void testDuplicateExtraCursor();
    void testEmptyTextKeysDontCorruptPreviousTyping();
    void testCompletionPopupSiblingSurvivesEditorSwap();
    void testCompletionPopupDoesNotStealFocus();
};

#endif // TEST_CODEEDITOR_MULTICURSOR_H