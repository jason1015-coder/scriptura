#ifndef TEST_BOOKMARKMANAGER_H
#define TEST_BOOKMARKMANAGER_H

#include <QObject>

class TestBookmarkManager : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void testInitialState();
    void testToggleAddAndRemove();
    void testToggleExistingRemoves();
    void testRemoveBookmark();
    void testRemoveAllBookmarks();
    void testRemoveAllBookmarksForFile();
    void testIsBookmarked();
    void testBookmarkAt();
    void testBookmarksForFile();
    void testGoToBookmark();
    void testNextPreviousBookmarkEmpty();
    void testUniqueIds();
    // Regression tests for the MANUAL_TEST_LOG fixes
    void testNextBookmarkScopedToFile();
    void testPreviousBookmarkScopedToFile();
    void testNavigateToMovesCaret();
    void testNavigationWrapsWithinFile();
};

#endif // TEST_BOOKMARKMANAGER_H
