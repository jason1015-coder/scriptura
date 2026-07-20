#include <QTest>
#include "sqliteviewer.h"
#include "test_sqliteviewer.h"

void TestSqliteViewer::testInitialState()
{
    SqliteViewerPanel panel;
    QVERIFY(panel.isEnabled());
    // No database file set initially
    panel.refresh(); // should not crash
}

void TestSqliteViewer::testRefreshWithoutDb()
{
    SqliteViewerPanel panel;
    // refresh() with no db file should clear UI, not crash
    panel.refresh();
    QVERIFY(true); // reached without crash
}
