#include <QTest>
#include <QSignalSpy>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QFile>
#include "largefilehandler.h"
#include "test_largefilehandler.h"

static QString writeTempFile(const QString &dir, const QString &name, int lineCount)
{
    QString path = dir + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return QString();
    for (int i = 0; i < lineCount; ++i) {
        f.write(QString("line %1\n").arg(i).toUtf8());
    }
    f.close();
    return path;
}

void TestLargeFileHandler::testInitialState()
{
    QPlainTextEdit editor;
    LargeFileHandler lfh(&editor);
    QVERIFY(!lfh.isLargeFile());
    QCOMPARE(lfh.largeFileThreshold(), 10000);
    QCOMPARE(lfh.chunkSize(), 1000);
    QCOMPARE(lfh.highlightChunkSize(), 500);
    QCOMPARE(lfh.totalLines(), 0);
    QCOMPARE(lfh.loadedLines(), 0);
    QCOMPARE(lfh.loadProgress(), 0.0);
}

void TestLargeFileHandler::testConfigSetters()
{
    QPlainTextEdit editor;
    LargeFileHandler lfh(&editor);
    lfh.setLargeFileThreshold(50);
    lfh.setChunkSize(10);
    lfh.setHighlightChunkSize(5);
    QCOMPARE(lfh.largeFileThreshold(), 50);
    QCOMPARE(lfh.chunkSize(), 10);
    QCOMPARE(lfh.highlightChunkSize(), 5);
}

void TestLargeFileHandler::testLoadNonexistentFile()
{
    QPlainTextEdit editor;
    LargeFileHandler lfh(&editor);
    QVERIFY(!lfh.loadLargeFile("/nonexistent/definitely_missing.txt"));
    QVERIFY(!lfh.isLargeFile());
}

void TestLargeFileHandler::testLoadSmallFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir.path(), "small.txt", 5);

    QPlainTextEdit editor;
    LargeFileHandler lfh(&editor);
    lfh.setLargeFileThreshold(10000); // 5 < 10000 => normal load

    QSignalSpy loaded(&lfh, &LargeFileHandler::fileLoaded);
    QVERIFY(lfh.loadLargeFile(path));
    QCOMPARE(loaded.count(), 1);
    QCOMPARE(loaded.first().first().toInt(), 5);

    QVERIFY(!lfh.isLargeFile());
    QCOMPARE(lfh.totalLines(), 5);
    QCOMPARE(lfh.loadedLines(), 5);
    QCOMPARE(lfh.loadProgress(), 1.0);
    QVERIFY(editor.toPlainText().contains("line 0"));
    QVERIFY(editor.toPlainText().contains("line 4"));
}

void TestLargeFileHandler::testLoadLargeFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir.path(), "big.txt", 20);

    QPlainTextEdit editor;
    LargeFileHandler lfh(&editor);
    lfh.setLargeFileThreshold(10); // 20 > 10 => virtual scrolling
    lfh.setChunkSize(4);

    QSignalSpy loaded(&lfh, &LargeFileHandler::fileLoaded);
    QVERIFY(lfh.loadLargeFile(path));
    QCOMPARE(loaded.count(), 1);
    QCOMPARE(loaded.first().first().toInt(), 20);

    QVERIFY(lfh.isLargeFile());
    QCOMPARE(lfh.totalLines(), 20);
    // Only the first chunk (4 lines) is loaded initially.
    QCOMPARE(lfh.loadedLines(), 4);
    QVERIFY(editor.toPlainText().contains("line 0"));
}

void TestLargeFileHandler::testLoadProgress()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir.path(), "progress.txt", 20);

    QPlainTextEdit editor;
    LargeFileHandler lfh(&editor);
    lfh.setLargeFileThreshold(10);
    lfh.setChunkSize(4);

    QSignalSpy progress(&lfh, &LargeFileHandler::loadProgressChanged);
    QVERIFY(lfh.loadLargeFile(path));

    QVERIFY(progress.count() >= 1);
    // 4 / 20 = 0.2
    QVERIFY(qAbs(lfh.loadProgress() - 0.2) < 0.001);
}

void TestLargeFileHandler::testUnload()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir.path(), "unload.txt", 20);

    QPlainTextEdit editor;
    LargeFileHandler lfh(&editor);
    lfh.setLargeFileThreshold(10);
    lfh.setChunkSize(4);
    QVERIFY(lfh.loadLargeFile(path));
    QVERIFY(lfh.isLargeFile());

    QSignalSpy unloaded(&lfh, &LargeFileHandler::fileUnloaded);
    lfh.unloadFile();
    QCOMPARE(unloaded.count(), 1);
    QVERIFY(!lfh.isLargeFile());
    QCOMPARE(lfh.totalLines(), 0);
    QCOMPARE(lfh.loadedLines(), 0);
    QCOMPARE(lfh.loadProgress(), 0.0);
    QVERIFY(editor.toPlainText().isEmpty());
}

void TestLargeFileHandler::testEnsureLineLoaded()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir.path(), "ensure.txt", 20);

    QPlainTextEdit editor;
    LargeFileHandler lfh(&editor);
    lfh.setLargeFileThreshold(10);
    lfh.setChunkSize(4);
    QVERIFY(lfh.loadLargeFile(path));
    QCOMPARE(lfh.loadedLines(), 4);

    // Request a line in a chunk that is not yet loaded (line 10 -> chunk 10).
    lfh.ensureLineLoaded(10);
    QCOMPARE(lfh.loadedLines(), 8);

    // Requesting a line that is already loaded should not load more.
    lfh.ensureLineLoaded(0);
    QCOMPARE(lfh.loadedLines(), 8);
}

void TestLargeFileHandler::testSetVisibleRange()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir.path(), "visible.txt", 20);

    QPlainTextEdit editor;
    LargeFileHandler lfh(&editor);
    lfh.setLargeFileThreshold(10);
    lfh.setChunkSize(4);
    QVERIFY(lfh.loadLargeFile(path));

    // Should not crash and should load chunks for the visible lines.
    lfh.setVisibleRange(0, 5);
    QVERIFY(lfh.loadedLines() >= 4);

    // Non-large file: setVisibleRange is a no-op guard.
    QPlainTextEdit smallEditor;
    LargeFileHandler lfhSmall(&smallEditor);
    lfhSmall.setLargeFileThreshold(10000);
    QVERIFY(lfhSmall.loadLargeFile(writeTempFile(dir.path(), "s.txt", 3)));
    lfhSmall.setVisibleRange(0, 2); // no crash
}

void TestLargeFileHandler::testChunkLoading()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeTempFile(dir.path(), "chunks.txt", 20);

    QPlainTextEdit editor;
    LargeFileHandler lfh(&editor);
    lfh.setLargeFileThreshold(10);
    lfh.setChunkSize(4);

    QSignalSpy chunkLoaded(&lfh, &LargeFileHandler::chunkLoaded);
    QVERIFY(lfh.loadLargeFile(path));
    QVERIFY(chunkLoaded.count() >= 1);
    QCOMPARE(chunkLoaded.first().at(0).toInt(), 0); // first chunk starts at line 0
}
