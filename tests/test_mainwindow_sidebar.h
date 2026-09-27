#ifndef TEST_MAINWINDOW_SIDEBAR_H
#define TEST_MAINWINDOW_SIDEBAR_H

#include <QObject>

// Integration tests for the sidebar drawer: the file tree drops its folder
// glyphs while the drawer is drawn out, and the collapse state that closeEvent
// persists is the real one (the drawer is collapsed by width, never by hiding,
// so isHidden() used to always report "collapsed" on quit).
class TestMainWindowSidebar : public QObject
{
    Q_OBJECT
private slots:
    void testFolderIconsFollowDrawerState();
    void testCollapseStatePersistsOnClose();
    void testCollapseStatePersistsWhenLeftOpen();
    void tmpShotDiagnostic();
};

#endif // TEST_MAINWINDOW_SIDEBAR_H
