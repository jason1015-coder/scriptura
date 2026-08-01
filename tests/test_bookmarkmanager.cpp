#include <QTest>
#include <QSignalSpy>
#include <QSettings>
#include "bookmarkmanager.h"
#include "test_bookmarkmanager.h"

void TestBookmarkManager::init()
{
    // BookmarkManager loads persisted bookmarks from QSettings in its
    // constructor; clear between tests so they don't leak into each other.
    QSettings().clear();
}

void TestBookmarkManager::testInitialState()
{
    BookmarkManager mgr;
    QCOMPARE(mgr.bookmarkCount(), 0);
    QVERIFY(mgr.bookmarks().isEmpty());
}

void TestBookmarkManager::testToggleAddAndRemove()
{
    BookmarkManager mgr;
    QSignalSpy toggled(&mgr, &BookmarkManager::bookmarkToggled);
    QSignalSpy changed(&mgr, &BookmarkManager::bookmarksChanged);

    int id = mgr.toggleBookmark("/tmp/test.cpp", 5, "int x = 5;");
    QVERIFY(id >= 0);
    QCOMPARE(mgr.bookmarkCount(), 1);
    QCOMPARE(toggled.count(), 1);
    QCOMPARE(changed.count(), 1);
}

void TestBookmarkManager::testToggleExistingRemoves()
{
    BookmarkManager mgr;
    int id = mgr.toggleBookmark("/tmp/a.cpp", 3, "text");
    QVERIFY(id >= 0);

    int result = mgr.toggleBookmark("/tmp/a.cpp", 3, "text");
    QCOMPARE(result, -1); // toggled off
    QCOMPARE(mgr.bookmarkCount(), 0);
}

void TestBookmarkManager::testRemoveBookmark()
{
    BookmarkManager mgr;
    int id = mgr.toggleBookmark("/tmp/a.cpp", 1, "one");
    QSignalSpy removed(&mgr, &BookmarkManager::bookmarkToggled);
    mgr.removeBookmark(id);
    QCOMPARE(mgr.bookmarkCount(), 0);
    QCOMPARE(removed.count(), 1);

    // Removing a non-existent id should be a no-op
    mgr.removeBookmark(999);
    QCOMPARE(mgr.bookmarkCount(), 0);
}

void TestBookmarkManager::testRemoveAllBookmarks()
{
    BookmarkManager mgr;
    mgr.toggleBookmark("/tmp/a.cpp", 1, "a");
    mgr.toggleBookmark("/tmp/b.cpp", 2, "b");
    mgr.removeAllBookmarks();
    QCOMPARE(mgr.bookmarkCount(), 0);

    // No-op when already empty
    mgr.removeAllBookmarks();
    QCOMPARE(mgr.bookmarkCount(), 0);
}

void TestBookmarkManager::testRemoveAllBookmarksForFile()
{
    BookmarkManager mgr;
    mgr.toggleBookmark("/tmp/a.cpp", 1, "a1");
    mgr.toggleBookmark("/tmp/a.cpp", 2, "a2");
    mgr.toggleBookmark("/tmp/b.cpp", 3, "b1");
    mgr.removeAllBookmarksForFile("/tmp/a.cpp");
    QCOMPARE(mgr.bookmarkCount(), 1);
    QCOMPARE(mgr.bookmarks().first().filePath, QString("/tmp/b.cpp"));

    // No-op for file without bookmarks
    mgr.removeAllBookmarksForFile("/tmp/none.cpp");
    QCOMPARE(mgr.bookmarkCount(), 1);
}

void TestBookmarkManager::testIsBookmarked()
{
    BookmarkManager mgr;
    mgr.toggleBookmark("/tmp/a.cpp", 10, "line");
    QVERIFY(mgr.isBookmarked("/tmp/a.cpp", 10));
    QVERIFY(!mgr.isBookmarked("/tmp/a.cpp", 11));
    QVERIFY(!mgr.isBookmarked("/tmp/b.cpp", 10));
}

void TestBookmarkManager::testBookmarkAt()
{
    BookmarkManager mgr;
    int id = mgr.toggleBookmark("/tmp/a.cpp", 7, "line");
    QCOMPARE(mgr.bookmarkAt("/tmp/a.cpp", 7), id);
    QCOMPARE(mgr.bookmarkAt("/tmp/a.cpp", 8), -1);
}

void TestBookmarkManager::testBookmarksForFile()
{
    BookmarkManager mgr;
    mgr.toggleBookmark("/tmp/a.cpp", 1, "a1");
    mgr.toggleBookmark("/tmp/b.cpp", 2, "b1");
    mgr.toggleBookmark("/tmp/a.cpp", 3, "a2");
    QList<Bookmark> list = mgr.bookmarksForFile("/tmp/a.cpp");
    QCOMPARE(list.size(), 2);
}

void TestBookmarkManager::testGoToBookmark()
{
    BookmarkManager mgr;
    int id = mgr.toggleBookmark("/tmp/a.cpp", 4, "line");
    QSignalSpy nav(&mgr, &BookmarkManager::bookmarkNavigated);
    mgr.goToBookmark(id);
    QCOMPARE(nav.count(), 1);

    // Non-existent id: no signal
    mgr.goToBookmark(12345);
    QCOMPARE(nav.count(), 1);
}

void TestBookmarkManager::testNextPreviousBookmarkEmpty()
{
    BookmarkManager mgr;
    QSignalSpy nav(&mgr, &BookmarkManager::bookmarkNavigated);
    mgr.nextBookmark();
    mgr.previousBookmark();
    QCOMPARE(nav.count(), 0);
}

void TestBookmarkManager::testUniqueIds()
{
    BookmarkManager mgr;
    int id1 = mgr.toggleBookmark("/tmp/a.cpp", 1, "a");
    int id2 = mgr.toggleBookmark("/tmp/a.cpp", 2, "b");
    int id3 = mgr.toggleBookmark("/tmp/a.cpp", 3, "c");
    QVERIFY(id1 != id2);
    QVERIFY(id2 != id3);
    QVERIFY(id1 != id3);
}
