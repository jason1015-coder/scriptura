#ifndef TEST_BOOKMARKPANEL_H
#define TEST_BOOKMARKPANEL_H

#include <QObject>

class TestBookmarkPanel : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void testSetManager();
    void testJumpActivatesSignal();
    void testDoubleClickJump();
    void testRemoveBookmark();
    void testClearAll();
    void testEmptyManager();
    void testManagerSwitchRefreshes();
};

#endif // TEST_BOOKMARKPANEL_H
