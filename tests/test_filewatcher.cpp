#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include "filewatcher.h"
#include "test_filewatcher.h"

static QString writeFile(const QString &path, const QByteArray &content)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return QString();
    f.write(content);
    f.close();
    return path;
}

void TestFileWatcher::testInitialState()
{
    FileWatcher fw;
    QVERIFY(fw.watchedFiles().isEmpty());
    QCOMPARE(fw.debounceInterval(), 500);
    QVERIFY(!fw.autoReload());
}

void TestFileWatcher::testWatchNonexistentFile()
{
    FileWatcher fw;
    fw.watchFile("/nonexistent/missing.txt");
    QVERIFY(fw.watchedFiles().isEmpty());
    QVERIFY(!fw.isWatching("/nonexistent/missing.txt"));
}

void TestFileWatcher::testWatchAndUnwatch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeFile(dir.filePath("a.txt"), "hello");

    FileWatcher fw;
    fw.watchFile(path);
    QVERIFY(fw.isWatching(path));
    QCOMPARE(fw.watchedFiles().size(), 1);
    QVERIFY(fw.watchedFiles().contains(path));

    fw.unwatchFile(path);
    QVERIFY(!fw.isWatching(path));
    QVERIFY(fw.watchedFiles().isEmpty());
}

void TestFileWatcher::testWatchDuplicate()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeFile(dir.filePath("a.txt"), "hello");

    FileWatcher fw;
    fw.watchFile(path);
    fw.watchFile(path); // second watch is ignored
    QCOMPARE(fw.watchedFiles().size(), 1);
}

void TestFileWatcher::testWatchedFilesList()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString a = writeFile(dir.filePath("a.txt"), "a");
    QString b = writeFile(dir.filePath("b.txt"), "b");

    FileWatcher fw;
    fw.watchFile(a);
    fw.watchFile(b);
    QCOMPARE(fw.watchedFiles().size(), 2);
}

void TestFileWatcher::testUnwatchAll()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString a = writeFile(dir.filePath("a.txt"), "a");
    QString b = writeFile(dir.filePath("b.txt"), "b");

    FileWatcher fw;
    fw.watchFile(a);
    fw.watchFile(b);
    QCOMPARE(fw.watchedFiles().size(), 2);
    fw.unwatchAllFiles();
    QVERIFY(fw.watchedFiles().isEmpty());
}

void TestFileWatcher::testHasChangesInitiallyFalse()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeFile(dir.filePath("a.txt"), "hello");

    FileWatcher fw;
    QVERIFY(!fw.hasChanges(path)); // not watched
    fw.watchFile(path);
    QVERIFY(!fw.hasChanges(path)); // watched but unchanged
}

void TestFileWatcher::testRefreshFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeFile(dir.filePath("a.txt"), "hello");

    FileWatcher fw;
    fw.watchFile(path);

    // Modify the file on disk, then refresh (re-reads baseline) — no change
    // should be reported afterwards because the baseline is updated.
    writeFile(path, "world");
    fw.refreshFile(path);
    QVERIFY(!fw.hasChanges(path));

    // Refresh of an unwatched file is a silent no-op.
    fw.refreshFile(dir.filePath("nope.txt"));
}

void TestFileWatcher::testConfigSetters()
{
    FileWatcher fw;
    fw.setDebounceInterval(250);
    QCOMPARE(fw.debounceInterval(), 250);
    fw.setAutoReload(true);
    QVERIFY(fw.autoReload());
}

void TestFileWatcher::testExternalChangeDetected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeFile(dir.filePath("a.txt"), "hello");

    FileWatcher fw;
    fw.watchFile(path);

    // Simulate an external edit: change the file and call the private
    // change handler directly via the meta-object.
    writeFile(path, "world");

    QSignalSpy changed(&fw, &FileWatcher::fileChangedExternally);
    QSignalSpy diff(&fw, &FileWatcher::fileChangedWithDiff);
    QMetaObject::invokeMethod(&fw, "onFileChanged", Qt::DirectConnection,
                              Q_ARG(QString, path));

    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.first().first().toString(), path);
    QCOMPARE(diff.count(), 1);
    QVERIFY(!diff.first().at(1).toString().isEmpty());
    QVERIFY(fw.hasChanges(path));
}

void TestFileWatcher::testNoChangeNoSignal()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = writeFile(dir.filePath("a.txt"), "same");

    FileWatcher fw;
    fw.watchFile(path);

    QSignalSpy changed(&fw, &FileWatcher::fileChangedExternally);
    // Content is unchanged -> no signal should fire.
    QMetaObject::invokeMethod(&fw, "onFileChanged", Qt::DirectConnection,
                              Q_ARG(QString, path));
    QCOMPARE(changed.count(), 0);
    QVERIFY(!fw.hasChanges(path));
}
