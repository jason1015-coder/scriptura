#include <QTest>
#include <QSignalSpy>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QTreeWidget>
#include <QApplication>
#include "internals/settings_store.h"
#include "bookmarkmanager.h"
#include "bookmarkpanel.h"
#include "test_bookmarkpanel.h"

void TestBookmarkPanel::init()
{
    SettingsStore::instance().clear();
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
    QVERIFY(itemRect.isValid());
    QVERIFY(tree->itemAt(itemRect.center()) == item);
    // The offscreen test platform does not synthesize itemDoubleClicked from
    // mouse events, so drive the panel's double-click handler directly. This
    // exercises the same production path (onItemDoubleClicked ->
    // onJumpClicked -> bookmarkActivated).
    tree->setCurrentItem(item);
    QVERIFY(QMetaObject::invokeMethod(&panel, "onItemDoubleClicked",
                                      Q_ARG(QTreeWidgetItem *, item),
                                      Q_ARG(int, 0)));

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

    // Managers persist through the shared SettingsStore; clear it so the
    // second manager starts empty like the first one did.
    SettingsStore::instance().clear();
    BookmarkManager mgr2;
    mgr2.toggleBookmark("/tmp/b.cpp", 2, "b");

    BookmarkPanelWidget panel(&mgr1);
    QCOMPARE(panel.bookmarkCount(), 1);

    panel.setManager(&mgr2);
    QCOMPARE(panel.bookmarkCount(), 1);

    QTreeWidget *tree = panel.findChild<QTreeWidget*>();
    QTreeWidgetItem *group = tree->topLevelItem(0);
    QVERIFY(group != nullptr);
    // Group headers show the file name (see BookmarkPanelWidget::populateTree).
    QCOMPARE(group->text(0), QString("b.cpp"));
}
