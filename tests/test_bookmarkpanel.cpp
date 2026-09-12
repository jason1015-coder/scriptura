#include <QTest>
#include <QSignalSpy>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QTreeWidget>
#include <QApplication>
#include "bookmarkmanager.h"
#include "bookmarkpanel.h"
#include "test_bookmarkpanel.h"

void TestBookmarkPanel::init()
{
    QSettings().clear();
}

void TestBookmarkPanel::testSetManager()
{
    BookmarkManager mgr;
    BookmarkPanelWidget panel(nullptr);

    panel.setManager(&mgr);
    QCOMPARE(panel.bookmarkCount(), 0);

    mgr.toggleBookmark("/tmp/test.cpp", 5, "int x = 5;");
    panel.setManager(&mgr);
    QCOMPARE(panel.bookmarkCount(), 1);
}

void TestBookmarkPanel::testJumpActivatesSignal()
{
    BookmarkManager mgr;
    mgr.toggleBookmark("/tmp/test.cpp", 5, "int x = 5;");
    BookmarkPanelWidget panel(&mgr);

    QSignalSpy activatedSpy(&panel, &BookmarkPanelWidget::bookmarkActivated);

    QTreeWidget *tree = panel.findChild<QTreeWidget*>();
    QVERIFY(tree != nullptr);

    QTreeWidgetItem *group = tree->topLevelItem(0);
    QVERIFY(group != nullptr);
    QTreeWidgetItem *item = group->child(0);
    QVERIFY(item != nullptr);

    tree->setCurrentItem(item);

    QList<QPushButton*> buttons = panel.findChildren<QPushButton*>();
    QPushButton *jumpBtn = nullptr;
    for (QPushButton *btn : buttons) {
        if (btn->text() == "Jump") {
            jumpBtn = btn;
            break;
        }
    }
    QVERIFY(jumpBtn != nullptr);
    QTest::mouseClick(jumpBtn, Qt::LeftButton);

    QCOMPARE(activatedSpy.count(), 1);
    QCOMPARE(activatedSpy.first().first().toString(), QString("/tmp/test.cpp"));
    QCOMPARE(activatedSpy.first().at(1).toInt(), 5);
    QCOMPARE(mgr.bookmarkCount(), 1);
}

void TestBookmarkPanel::testDoubleClickJump()
{
    BookmarkManager mgr;
    mgr.toggleBookmark("/tmp/test.cpp", 5, "int x = 5;");
    BookmarkPanelWidget panel(&mgr);

    QSignalSpy activatedSpy(&panel, &BookmarkPanelWidget::bookmarkActivated);

    QTreeWidget *tree = panel.findChild<QTreeWidget*>();
    QVERIFY(tree != nullptr);

    QTreeWidgetItem *group = tree->topLevelItem(0);
    QVERIFY(group != nullptr);
    QTreeWidgetItem *item = group->child(0);
    QVERIFY(item != nullptr);

    QRect itemRect = tree->visualItemRect(item);
    QPoint clickPos(itemRect.center().x(), itemRect.center().y());
    // Click twice quickly to simulate double-click
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::KeyboardModifiers(), clickPos);
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::KeyboardModifiers(), clickPos);

    QCOMPARE(activatedSpy.count(), 1);
    QCOMPARE(activatedSpy.first().first().toString(), QString("/tmp/test.cpp"));
}

void TestBookmarkPanel::testRemoveBookmark()
{
    BookmarkManager mgr;
    mgr.toggleBookmark("/tmp/test.cpp", 5, "int x = 5;");
    BookmarkPanelWidget panel(&mgr);

    QSignalSpy removedSpy(&panel, &BookmarkPanelWidget::bookmarkRemoved);

    QTreeWidget *tree = panel.findChild<QTreeWidget*>();
    QTreeWidgetItem *group = tree->topLevelItem(0);
    QTreeWidgetItem *item = group->child(0);
    tree->setCurrentItem(item);

    QList<QPushButton*> buttons = panel.findChildren<QPushButton*>();
    QPushButton *removeBtn = nullptr;
    for (QPushButton *btn : buttons) {
        if (btn->text() == "Remove") {
            removeBtn = btn;
            break;
        }
    }
    QVERIFY(removeBtn != nullptr);
    QTest::mouseClick(removeBtn, Qt::LeftButton);

    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(mgr.bookmarkCount(), 0);
}

void TestBookmarkPanel::testClearAll()
{
    BookmarkManager mgr;
    mgr.toggleBookmark("/tmp/a.cpp", 1, "a");
    mgr.toggleBookmark("/tmp/b.cpp", 2, "b");
    BookmarkPanelWidget panel(&mgr);

    QCOMPARE(panel.bookmarkCount(), 2);

    QList<QPushButton*> buttons = panel.findChildren<QPushButton*>();
    QPushButton *clearBtn = nullptr;
    for (QPushButton *btn : buttons) {
        if (btn->text() == "Clear All") {
            clearBtn = btn;
            break;
        }
    }
    QVERIFY(clearBtn != nullptr);
    QTest::mouseClick(clearBtn, Qt::LeftButton);

    QCOMPARE(mgr.bookmarkCount(), 0);
}

void TestBookmarkPanel::testEmptyManager()
{
    BookmarkPanelWidget panel(nullptr);
    QCOMPARE(panel.bookmarkCount(), 0);

    QSignalSpy activatedSpy(&panel, &BookmarkPanelWidget::bookmarkActivated);

    QList<QPushButton*> buttons = panel.findChildren<QPushButton*>();
    for (QPushButton *btn : buttons) {
        if (btn->text() == "Jump") {
            QTest::mouseClick(btn, Qt::LeftButton);
            break;
        }
    }

    QCOMPARE(activatedSpy.count(), 0);
}

void TestBookmarkPanel::testManagerSwitchRefreshes()
{
    BookmarkManager mgr1;
    mgr1.toggleBookmark("/tmp/a.cpp", 1, "a");

    BookmarkManager mgr2;
    mgr2.toggleBookmark("/tmp/b.cpp", 2, "b");

    BookmarkPanelWidget panel(&mgr1);
    QCOMPARE(panel.bookmarkCount(), 1);

    panel.setManager(&mgr2);
    QCOMPARE(panel.bookmarkCount(), 1);

    QTreeWidget *tree = panel.findChild<QTreeWidget*>();
    QTreeWidgetItem *group = tree->topLevelItem(0);
    QVERIFY(group != nullptr);
    QCOMPARE(group->text(0), QString("/tmp/b.cpp"));
}
