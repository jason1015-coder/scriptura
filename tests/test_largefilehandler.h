#ifndef TEST_LARGEFILEHANDLER_H
#define TEST_LARGEFILEHANDLER_H

#include <QObject>

class TestLargeFileHandler : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testConfigSetters();
    void testLoadNonexistentFile();
    void testLoadSmallFile();
    void testLoadLargeFile();
    void testLoadProgress();
    void testUnload();
    void testEnsureLineLoaded();
    void testSetVisibleRange();
    void testChunkLoading();
};

#endif // TEST_LARGEFILEHANDLER_H
