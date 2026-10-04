#include <QTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QTabBar>
#include <QTabWidget>
#include <QStackedWidget>
#include <QToolButton>
#include <QMenu>
#include <QFile>
#include <QTextStream>
#include <QWidget>

#include "mainwindow.h"
#include "test_mainwindow_panels.h"

namespace {

// Every panel the app registers. They are opened on demand from the "+" menu, so
// this list is what the menu has to offer.
const QStringList kBuiltInPanels = {
    QStringLiteral("Search"),      QStringLiteral("Rebase"),    QStringLiteral("Tasks"),
    QStringLiteral("Terminal"),    QStringLiteral("Bookmarks"), QStringLiteral("Marketplace"),
};

QString writeTempFile(QTemporaryDir &dir, const QString &name, const QString &contents)
{
    QFile f(dir.filePath(name));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    QTextStream out(&f);
    out << contents;
    return dir.filePath(name);
}

// A panel tab is a tabBar tab whose data is "panel:<panelIndex>".
int panelTabIndex(QTabBar *tabBar, int panelIndex)
{
    for (int i = 0; i < tabBar->count(); ++i) {
        const QVariant data = tabBar->tabData(i);
        if (data.typeId() == QMetaType::QString
            && data.toString() == QString("panel:%1").arg(panelIndex)) {
            return i;
        }
    }
    return -1;
}

// The title of the panel whose tab sits at `tabIndex`, or an empty string.
QString panelTitleForTab(QTabBar *tabBar, int tabIndex)
{
    const QVariant data = tabBar->tabData(tabIndex);
    if (data.typeId() == QMetaType::QString && data.toString().startsWith("panel:"))
        return tabBar->tabText(tabIndex);
    return {};
}

QStringList openPanelTabTitles(QTabBar *tabBar)
{
    QStringList titles;
    for (int i = 0; i < tabBar->count(); ++i) {
        const QString title = panelTitleForTab(tabBar, i);
        if (!title.isEmpty())
            titles << title;
    }
    return titles;
}

int fileTabIndex(QTabBar *tabBar)
{
    for (int i = 0; i < tabBar->count(); ++i) {
        const QVariant data = tabBar->tabData(i);
        if (data.typeId() == QMetaType::QString && !data.toString().startsWith("panel:"))
            return i;
    }
    return -1;
}

struct Fixture {
    QTemporaryDir dir;
    MainWindow *win = nullptr;
    QTabBar *tabBar = nullptr;
    QStackedWidget *editorStack = nullptr;

    bool create(const QString &projectFile = QStringLiteral("sample.txt"))
    {
        if (!dir.isValid())
            return false;
        const QString filePath = writeTempFile(dir, projectFile, QStringLiteral("hello\n"));
        if (filePath.isEmpty())
            return false;
        win = new MainWindow(dir.path(), QStringList{filePath});
        win->show();
        QTest::qWait(50);
        QCoreApplication::processEvents();
        tabBar = win->findChild<QTabBar*>(QStringLiteral("tabBar"));
        editorStack = win->findChild<QStackedWidget*>(QStringLiteral("editorStack"));
        return tabBar && editorStack;
    }

    ~Fixture() { delete win; }

    QWidget *editorPage() const
    {
        return editorStack->findChild<QTabWidget*>(QStringLiteral("tabWidget"),
                                                  Qt::FindDirectChildrenOnly);
    }

    int panelIndex(const QString &title) const { return win->findPanelIndex(title); }
};

// Opens the "+" menu the way a click does and hands back the menu.
QMenu *openPlusMenu(MainWindow *win)
{
    QToolButton *plus = win->findChild<QToolButton*>(QStringLiteral("newTabButton"));
    if (!plus)
        return nullptr;
    QTest::mouseClick(plus, Qt::LeftButton);
    QCoreApplication::processEvents();
    return win->findChild<QMenu*>(QStringLiteral("newTabPanel"));
}

QAction *actionIn(QMenu *menu, const QString &text)
{
    for (QAction *action : menu->actions()) {
        if (action->text() == text)
            return action;
    }
    return nullptr;
}

QStringList entryTexts(QMenu *menu)
{
    QStringList entries;
    for (const QAction *action : menu->actions()) {
        if (!action->isSeparator())
            entries << action->text();
    }
    return entries;
}

} // namespace

void TestMainWindowPanels::testNoPanelTabsArePreopened()
{
    Fixture fx;
    QVERIFY(fx.create());

    // Only the file that was opened may have a tab: panels are available from
    // the "+" menu, not opened in advance.
    QVERIFY2(openPanelTabTitles(fx.tabBar).isEmpty(),
             qPrintable(QStringLiteral("panels were pre-opened: %1")
                            .arg(openPanelTabTitles(fx.tabBar).join(QStringLiteral(", ")))));
    QCOMPARE(fx.tabBar->count(), 1);
    QVERIFY(fileTabIndex(fx.tabBar) >= 0);

    // Registering a panel must not add a page either — the terminal shell is not
    // started until the tab is actually opened.
    const int pagesBefore = fx.editorStack->count();
    QVERIFY(fx.panelIndex(QStringLiteral("Terminal")) >= 0);
    QCOMPARE(fx.editorStack->count(), pagesBefore);
}

void TestMainWindowPanels::testPlusMenuOffersEveryTabKind()
{
    Fixture fx;
    QVERIFY(fx.create());

    QMenu *menu = openPlusMenu(fx.win);
    QVERIFY2(menu, "the + button must open the tab-kind menu");
    const QStringList entries = entryTexts(menu);

    // The three non-panel kinds...
    QVERIFY(entries.contains(QStringLiteral("New Empty File")));
    QVERIFY(entries.contains(QStringLiteral("Open File...")));
    QVERIFY(entries.contains(QStringLiteral("Settings")));

    // ...and every panel, listed by name.
    for (const QString &title : kBuiltInPanels) {
        QVERIFY2(entries.contains(title),
                 qPrintable(QStringLiteral("the + menu is missing %1").arg(title)));
    }
    QCOMPARE(entries.size(), kBuiltInPanels.size() + 3);

    // Nothing is open yet, so no panel entry is ticked.
    for (const QString &title : kBuiltInPanels) {
        QAction *action = actionIn(menu, title);
        QVERIFY(action);
        QVERIFY2(!action->isChecked(),
                 qPrintable(QStringLiteral("%1 is ticked before being opened").arg(title)));
    }

    // There is no bottom panel to offer.
    QVERIFY(!fx.win->findChild<QWidget*>(QStringLiteral("bottomPanelContainer")));

    menu->close();
}

void TestMainWindowPanels::testPanelTabFillsContentArea()
{
    Fixture fx;
    QVERIFY(fx.create());

    const int panelIndex = fx.panelIndex(QStringLiteral("Search"));
    QVERIFY(panelIndex >= 0);
    fx.win->openPanelAsTab(panelIndex);
    QCoreApplication::processEvents();

    // Opening it is what creates the tab.
    const int tabIndex = panelTabIndex(fx.tabBar, panelIndex);
    QVERIFY(tabIndex >= 0);
    QCOMPARE(fx.tabBar->currentIndex(), tabIndex);

    QWidget *panel = fx.editorStack->currentWidget();
    QVERIFY(panel);
    QVERIFY2(panel != fx.editorPage(), "the panel must be the page on screen, not the editors");
    QVERIFY(panel->isVisible());

    // "Fills the tab completely": the panel is stretched over the whole page.
    // Geometry is compared in each widget's own parent coordinates.
    QVERIFY2(panel->pos() == QPoint(0, 0) && panel->size() == fx.editorStack->size(),
             qPrintable(QStringLiteral("panel is %1,%2 %3x%4 but the content area is %5x%6")
                            .arg(panel->x()).arg(panel->y())
                            .arg(panel->width()).arg(panel->height())
                            .arg(fx.editorStack->width()).arg(fx.editorStack->height())));
}

void TestMainWindowPanels::testPlusMenuOpensPanelAsTab()
{
    Fixture fx;
    QVERIFY(fx.create());

    QMenu *menu = openPlusMenu(fx.win);
    QVERIFY(menu);
    QAction *terminal = actionIn(menu, QStringLiteral("Terminal"));
    QVERIFY2(terminal, "the + menu must offer the terminal tab");
    terminal->trigger();
    QCoreApplication::processEvents();

    const int panelIndex = fx.panelIndex(QStringLiteral("Terminal"));
    QVERIFY(panelIndex >= 0);

    // The terminal became a page of the content area, and its tab is selected.
    QWidget *panel = fx.editorStack->currentWidget();
    QVERIFY(panel);
    QCOMPARE(panel->metaObject()->className(), QByteArray("TerminalPanel"));
    QCOMPARE(fx.tabBar->currentIndex(), panelTabIndex(fx.tabBar, panelIndex));

    // Reopening the menu now ticks it.
    menu->close();
    QMenu *again = openPlusMenu(fx.win);
    QVERIFY(again);
    QVERIFY(actionIn(again, QStringLiteral("Terminal"))->isChecked());
    QVERIFY(!actionIn(again, QStringLiteral("Search"))->isChecked());
    again->close();
}

void TestMainWindowPanels::testFileTabTakesContentAreaBack()
{
    Fixture fx;
    QVERIFY(fx.create());

    const int panelIndex = fx.panelIndex(QStringLiteral("Search"));
    QVERIFY(panelIndex >= 0);
    fx.win->openPanelAsTab(panelIndex);

    // Clicking a file tab brings the editors back; the panel keeps its tab.
    const int fileIndex = fileTabIndex(fx.tabBar);
    QVERIFY(fileIndex >= 0);
    fx.tabBar->setCurrentIndex(fileIndex);
    QCoreApplication::processEvents();

    QVERIFY(fx.editorPage());
    QCOMPARE(fx.editorStack->currentWidget(), fx.editorPage());
    QVERIFY2(panelTabIndex(fx.tabBar, panelIndex) >= 0,
             "switching to a file must not close the panel tab");
}

void TestMainWindowPanels::testClosingPanelTabKeepsPanelInMenu()
{
    Fixture fx;
    QVERIFY(fx.create());

    const int panelIndex = fx.panelIndex(QStringLiteral("Search"));
    QVERIFY(panelIndex >= 0);
    fx.win->openPanelAsTab(panelIndex);
    QWidget *panel = fx.editorStack->currentWidget();
    QVERIFY(panel);

    const int tabsBefore = fx.tabBar->count();
    const int pagesBefore = fx.editorStack->count();
    fx.win->closePanelTab(panelIndex);
    QCoreApplication::processEvents();

    QCOMPARE(fx.tabBar->count(), tabsBefore - 1);
    QCOMPARE(panelTabIndex(fx.tabBar, panelIndex), -1);
    QCOMPARE(fx.editorStack->count(), pagesBefore - 1);
    QVERIFY2(fx.editorStack->indexOf(panel) < 0,
             "a closed panel must not stay parented to the content area");
    QVERIFY(panel->isHidden());

    // Closing is not unregistering: the panel is still on offer, until it is
    // opened again.
    QMenu *menu = openPlusMenu(fx.win);
    QVERIFY(menu);
    QAction *search = actionIn(menu, QStringLiteral("Search"));
    QVERIFY2(search, "a closed panel must still be listed in the + menu");
    QVERIFY(!search->isChecked());
    QVERIFY(actionIn(menu, QStringLiteral("Terminal")));
    menu->close();
}

void TestMainWindowPanels::testReopenedPanelGetsItsTabBack()
{
    Fixture fx;
    QVERIFY(fx.create());

    const int searchIndex = fx.panelIndex(QStringLiteral("Search"));
    const int terminalIndex = fx.panelIndex(QStringLiteral("Terminal"));
    QVERIFY(searchIndex >= 0 && terminalIndex >= 0);
    fx.win->openPanelAsTab(searchIndex);
    fx.win->openPanelAsTab(terminalIndex);
    QCOMPARE(openPanelTabTitles(fx.tabBar).size(), 2);

    fx.win->closePanelTab(searchIndex);
    QCoreApplication::processEvents();

    // Reopening gives the panel a fresh tab next to the one still open, and the
    // other panel keeps its own (panel indices never shift).
    fx.win->openPanelAsTab(searchIndex);
    QCoreApplication::processEvents();
    const QStringList titles = openPanelTabTitles(fx.tabBar);
    QCOMPARE(titles.size(), 2);
    QVERIFY(titles.contains(QStringLiteral("Search")));
    QVERIFY(titles.contains(QStringLiteral("Terminal")));
    QCOMPARE(panelTabIndex(fx.tabBar, terminalIndex) >= 0, true);
    QCOMPARE(fx.tabBar->currentIndex(), panelTabIndex(fx.tabBar, searchIndex));

    fx.tabBar->setCurrentIndex(panelTabIndex(fx.tabBar, terminalIndex));
    QCoreApplication::processEvents();
    QCOMPARE(fx.editorStack->currentWidget()->metaObject()->className(),
             QByteArray("TerminalPanel"));
}

void TestMainWindowPanels::testEveryPanelOpensAsTab()
{
    Fixture fx;
    QVERIFY(fx.create());

    for (const QString &title : kBuiltInPanels) {
        const int panelIndex = fx.panelIndex(title);
        QVERIFY2(panelIndex >= 0, qPrintable(QStringLiteral("no panel called %1").arg(title)));

        const int pagesBefore = fx.editorStack->count();
        const int tabsBefore = fx.tabBar->count();
        fx.win->openPanelAsTab(panelIndex);
        QCoreApplication::processEvents();

        QWidget *panel = fx.editorStack->currentWidget();
        QVERIFY2(panel && panel->isVisible(),
                 qPrintable(QStringLiteral("%1 did not become the visible page").arg(title)));
        QCOMPARE(panel->pos(), QPoint(0, 0));
        QCOMPARE(panel->size(), fx.editorStack->size());
        QCOMPARE(fx.tabBar->count(), tabsBefore + 1);
        QCOMPARE(panelTitleForTab(fx.tabBar, fx.tabBar->currentIndex()), title);

        // Opening the same panel again re-selects its tab, it does not stack a
        // second copy.
        fx.win->openPanelAsTab(panelIndex);
        QCOMPARE(fx.editorStack->count(), pagesBefore + 1);
        QCOMPARE(fx.tabBar->count(), tabsBefore + 1);

        fx.win->closePanelTab(panelIndex);
        QCoreApplication::processEvents();
        QCOMPARE(fx.editorStack->count(), pagesBefore);
        QCOMPARE(fx.tabBar->count(), tabsBefore);
    }
}

void TestMainWindowPanels::testTerminalShortcutOpensTerminalTab()
{
    Fixture fx;
    QVERIFY(fx.create());

    const int panelIndex = fx.panelIndex(QStringLiteral("Terminal"));
    QVERIFY(panelIndex >= 0);
    QCOMPARE(panelTabIndex(fx.tabBar, panelIndex), -1);

    // Ctrl+` is the terminal's only shortcut; it selects the terminal tab.
    const int before = fx.editorStack->count();
    QTest::keyClick(fx.win, Qt::Key_QuoteLeft, Qt::ControlModifier);
    QCoreApplication::processEvents();

    QCOMPARE(fx.editorStack->count(), before + 1);
    QCOMPARE(fx.tabBar->currentIndex(), panelTabIndex(fx.tabBar, panelIndex));
    QWidget *panel = fx.editorStack->currentWidget();
    QVERIFY(panel);
    QCOMPARE(panel->metaObject()->className(), QByteArray("TerminalPanel"));
}