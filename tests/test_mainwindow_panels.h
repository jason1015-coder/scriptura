#ifndef TEST_MAINWINDOW_PANELS_H
#define TEST_MAINWINDOW_PANELS_H

#include <QObject>

// Integration tests for panels as tabs. There is no bottom panel any more: every
// registered panel gets a tab in the top tab bar, is listed by the tab bar's
// "+" menu, and fills the whole content area when selected. These drive the real
// MainWindow, because the tab/page bookkeeping (editorStack, tabBar data, panel
// registry) only exists there.
class TestMainWindowPanels : public QObject
{
    Q_OBJECT
private slots:
    void testNoPanelTabsArePreopened();
    void testPlusMenuOffersEveryTabKind();
    void testPanelTabFillsContentArea();
    void testPlusMenuOpensPanelAsTab();
    void testFileTabTakesContentAreaBack();
    void testClosingPanelTabKeepsPanelInMenu();
    void testReopenedPanelGetsItsTabBack();
    void testEveryPanelOpensAsTab();
    void testTerminalShortcutOpensTerminalTab();
};

#endif // TEST_MAINWINDOW_PANELS_H