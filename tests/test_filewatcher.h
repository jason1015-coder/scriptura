#ifndef TEST_FILEWATCHER_H
#define TEST_FILEWATCHER_H

#include <QObject>

class TestFileWatcher : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testWatchNonexistentFile();
    void testWatchAndUnwatch();
    void testWatchDuplicate();
    void testWatchedFilesList();
    void testUnwatchAll();
    void testHasChangesInitiallyFalse();
    void testRefreshFile();
    void testConfigSetters();
    void testExternalChangeDetected();
    void testNoChangeNoSignal();
};

#endif // TEST_FILEWATCHER_H
