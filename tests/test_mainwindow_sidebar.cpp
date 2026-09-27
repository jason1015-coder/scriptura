#include <QTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTreeView>

#include "mainwindow.h"
#include "customtitlebar.h"
#include "internals/settings_store.h"
#include "test_mainwindow_sidebar.h"

namespace {

// Every case starts from an explicit collapse state: MainWindow reads
// "ui/sidebarCollapsed" in applyLayout() at construction, and the store is
// process-wide and shared between cases.
void seedCollapsedState(bool collapsed)
{
    SettingsStore::instance().setValue("ui/sidebarCollapsed", collapsed);
}

// Drive the real user path: the title bar button → CustomTitleBar signal →
// Rust ui-action bridge → setSidebarCollapsed(). The state settles
// synchronously; only the width animation is still running afterwards.
void clickDrawerToggle(MainWindow &win)
{
    CustomTitleBar *titleBar = win.findChild<CustomTitleBar *>();
    Q_ASSERT(titleBar && titleBar->sidebarToggleButton);
    titleBar->sidebarToggleButton->click();
    QCoreApplication::processEvents();
}

QTreeView *fileTreeOf(MainWindow &win)
{
    return win.findChild<QTreeView *>(QStringLiteral("fileTreeView"));
}

QFileSystemModel *fileModelOf(MainWindow &win)
{
    return qobject_cast<QFileSystemModel *>(fileTreeOf(win)->model());
}

bool rowHasIcon(MainWindow &win, const QString &path)
{
    QFileSystemModel *model = fileModelOf(win);
    return model->data(model->index(path), Qt::DecorationRole).isValid();
}

// A project with one subdirectory and one file, so the tree has both a folder
// row (glyph toggles) and a file row (must keep its glyph at all times).
void makeProject(QTemporaryDir &dir, QString *subDirPath, QString *filePath)
{
    const QString subDir = dir.filePath(QStringLiteral("assets"));
    QDir().mkpath(subDir);
    *subDirPath = subDir;

    QFile f(dir.filePath(QStringLiteral("sample.txt")));
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << "line one\n";
    }
    *filePath = dir.filePath(QStringLiteral("sample.txt"));
}

} // namespace

void TestMainWindowSidebar::testFolderIconsFollowDrawerState()
{
    seedCollapsedState(true);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString subDir, filePath;
    makeProject(dir, &subDir, &filePath);

    MainWindow win(dir.path(), QStringList{filePath});
    win.show();
    QTest::qWait(50);
    QCoreApplication::processEvents();

    // Collapsed: the drawer is a rail, so the tree keeps its folder glyph.
    QVERIFY(rowHasIcon(win, subDir));

    clickDrawerToggle(win); // drawn out
    QVERIFY2(!rowHasIcon(win, subDir),
             "an open drawer lists files straight away — no folder icon");
    QVERIFY2(rowHasIcon(win, filePath),
             "file rows must keep their icon regardless of the drawer state");

    clickDrawerToggle(win); // collapsed again
    QVERIFY(rowHasIcon(win, subDir));
}

void TestMainWindowSidebar::tmpShotDiagnostic()
{
    Q_INIT_RESOURCE(resources);

    seedCollapsedState(false); // start with the drawer already drawn out
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString subDir, filePath;
    makeProject(dir, &subDir, &filePath);
    QVERIFY(QDir(subDir).mkpath(QStringLiteral("nested")));
    QFile f(dir.filePath(QStringLiteral("alpha.txt")));
    if (f.open(QIODevice::WriteOnly)) f.close();

    MainWindow win(dir.path(), QStringList{filePath});
    win.resize(1100, 700);
    win.show();
    QTest::qWait(300);
    QCoreApplication::processEvents();

    QTreeView *view = fileTreeOf(win);
    view->expandAll();
    QCoreApplication::processEvents();
    QTest::qWait(100);

    win.grab().save(QStringLiteral("/tmp/window_full.png"));

    QWidget *drawer = win.findChild<QWidget *>(QStringLiteral("sidebarDrawer"));
    qInfo("A. at startup (expanded): drawer=%dpx max=%d min=%d tree=%dpx",
          drawer->width(), drawer->maximumWidth(), drawer->minimumWidth(),
          view->width());

    // Now exercise the toggle path: collapse, then draw it back out.
    clickDrawerToggle(win);
    QTest::qWait(400);
    QCoreApplication::processEvents();
    qInfo("B. after collapsing:      drawer=%dpx max=%d min=%d",
          drawer->width(), drawer->maximumWidth(), drawer->minimumWidth());

    clickDrawerToggle(win);
    QTest::qWait(400);
    QCoreApplication::processEvents();
    qInfo("C. after re-expanding:    drawer=%dpx max=%d min=%d tree=%dpx",
          drawer->width(), drawer->maximumWidth(), drawer->minimumWidth(),
          view->width());
    win.grab().save(QStringLiteral("/tmp/window_reexpanded.png"));

    // The earlier 48px reading came from a window that was never resized, so it
    // inherited the constructor's fit-to-small-offscreen-screen size. Repeat the
    // "start collapsed, then toggle open" case at a realistic window size.
    seedCollapsedState(true);
    MainWindow win2(dir.path(), QStringList{filePath});
    win2.resize(1100, 700);
    win2.show();
    QTest::qWait(300);
    QCoreApplication::processEvents();
    QWidget *drawer2 = win2.findChild<QWidget *>(QStringLiteral("sidebarDrawer"));
    QTreeView *view2 = fileTreeOf(win2);
    qInfo("D. win2 at startup (collapsed): drawer=%dpx", drawer2->width());
    clickDrawerToggle(win2);
    QTest::qWait(400);
    QCoreApplication::processEvents();
    qInfo("E. win2 after toggling open:    drawer=%dpx tree=%dpx",
          drawer2->width(), view2->width());
    win2.grab().save(QStringLiteral("/tmp/window2_open.png"));
}

void TestMainWindowSidebar::testCollapseStatePersistsOnClose()
{
    // Start expanded, then collapse it the way a user does. closeEvent() used
    // to read sidebarDrawer->isHidden(), which is never true for this drawer
    // (collapsing animates the width to zero, it never hides the widget), so
    // every quit persisted "expanded" and a collapsed drawer came back open.
    seedCollapsedState(false);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString subDir, filePath;
    makeProject(dir, &subDir, &filePath);

    MainWindow win(dir.path(), QStringList{filePath});
    win.show();
    QTest::qWait(50);

    clickDrawerToggle(win); // collapse
    QVERIFY(SettingsStore::instance().value("ui/sidebarCollapsed").toBool());

    // No document is modified, so close() runs straight through closeEvent().
    win.close();
    QCoreApplication::processEvents();

    QVERIFY2(SettingsStore::instance().value("ui/sidebarCollapsed").toBool(),
             "a drawer the user collapsed must stay collapsed across a restart");
}

void TestMainWindowSidebar::testCollapseStatePersistsWhenLeftOpen()
{
    seedCollapsedState(false);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString subDir, filePath;
    makeProject(dir, &subDir, &filePath);

    // Starts expanded from the stored setting, and closing without touching the
    // toggle must keep it that way.
    MainWindow win(dir.path(), QStringList{filePath});
    win.show();
    QTest::qWait(50);
    QVERIFY(!rowHasIcon(win, subDir));

    win.close();
    QCoreApplication::processEvents();

    QVERIFY2(!SettingsStore::instance().value("ui/sidebarCollapsed").toBool(),
             "the expanded drawer restored at startup must survive the session");
}
