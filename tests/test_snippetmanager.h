#ifndef TEST_SNIPPETMANAGER_H
#define TEST_SNIPPETMANAGER_H

#include <QObject>

class TestSnippetManager : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void testInitialState();
    void testAddSnippet();
    void testAddDuplicateIdIgnored();
    void testUpdateSnippet();
    void testUpdateMissingSnippet();
    void testRemoveSnippet();
    void testRemoveMissingSnippet();
    void testSnippetById();
    void testSnippetByIdMissing();
    void testSnippetsForLanguage();
    void testSnippetPrefixes();
    void testHasSnippetForPrefix();
    void testSnippetForPrefix();
    void testInsertSnippetNoEditor();
    void testInsertSnippetWithTabStops();
    void testTabStopNavigation();
    void testClearTabStops();
    void testImportExport();
    void testImportBadFile();
    void testImportInvalidJson();
    void testExportToBadPath();
    void testVariableSubstitution();
};

#endif // TEST_SNIPPETMANAGER_H
