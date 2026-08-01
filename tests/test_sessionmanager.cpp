#include <QTest>
#include <QSignalSpy>
#include <QMainWindow>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include "sessionmanager.h"
#include "codeeditor.h"
#include "test_sessionmanager.h"

void TestSessionManager::init()
{
    // Remove any session/hot-exit leftovers from a previous run so every
    // test starts from a clean slate. (QStandardPaths runs in test mode,
    // so this only touches the test-only data location.)
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile::remove(dataDir + "/session.json");
    QDir hotDir(dataDir + "/hotexit");
    if (hotDir.exists())
        hotDir.removeRecursively();
}

void TestSessionManager::testDefaults()
{
    QMainWindow win;
    QTabWidget tabs;
    SessionManager mgr(&win, &tabs);
    QVERIFY(mgr.autoSaveSession());
    QVERIFY(mgr.hotExitEnabled());
    QVERIFY(!mgr.hasSavedSession());
}

void TestSessionManager::testSaveSessionDisabled()
{
    QMainWindow win;
    QTabWidget tabs;
    SessionManager mgr(&win, &tabs);
    mgr.setAutoSaveSession(false);
    QSignalSpy saved(&mgr, &SessionManager::sessionSaved);
    mgr.saveSession();
    QCOMPARE(saved.count(), 0);
    QVERIFY(!mgr.hasSavedSession());
}

void TestSessionManager::testSaveAndRestoreRoundTrip()
{
    QMainWindow win;
    QTabWidget tabs;
    SessionManager mgr(&win, &tabs);

    QSignalSpy saved(&mgr, &SessionManager::sessionSaved);
    mgr.saveSession();
    QCOMPARE(saved.count(), 1);
    QVERIFY(mgr.hasSavedSession());

    QSignalSpy restored(&mgr, &SessionManager::sessionRestored);
    bool ok = mgr.restoreSession();
    QVERIFY(ok);
    QCOMPARE(restored.count(), 1);
}

void TestSessionManager::testSaveWithEditorState()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString filePath = dir.filePath("doc.txt");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("hello\nworld\n");
        f.close();
    }

    QMainWindow win;
    QTabWidget tabs;
    CodeEditor editor;
    editor.setFilePath(filePath);
    editor.setPlainText("hello\nworld");
    tabs.addTab(&editor, "doc.txt");
    tabs.setCurrentIndex(0);

    SessionManager mgr(&win, &tabs);
    mgr.saveSession();
    QVERIFY(mgr.hasSavedSession());

    // Restore should request the previously open file (it exists on disk).
    QSignalSpy requested(&mgr, &SessionManager::sessionFileRequested);
    bool ok = mgr.restoreSession();
    QVERIFY(ok);
    QVERIFY(requested.count() >= 1);
    QCOMPARE(requested.first().at(0).toString(), filePath);
}

void TestSessionManager::testRestoreNoSession()
{
    QMainWindow win;
    QTabWidget tabs;
    SessionManager mgr(&win, &tabs);
    QVERIFY(!mgr.hasSavedSession());
    QVERIFY(!mgr.restoreSession());
}

void TestSessionManager::testClearSession()
{
    QMainWindow win;
    QTabWidget tabs;
    SessionManager mgr(&win, &tabs);
    mgr.saveSession();
    QVERIFY(mgr.hasSavedSession());
    mgr.clearSession();
    QVERIFY(!mgr.hasSavedSession());
}

void TestSessionManager::testHotExitRoundTrip()
{
    QMainWindow win;
    QTabWidget tabs;
    CodeEditor editor; // untitled buffer (empty file path) -> hot-exit candidate
    editor.setPlainText("unsaved content");
    editor.document()->setModified(true);
    tabs.addTab(&editor, "untitled");

    SessionManager mgr(&win, &tabs);
    QSignalSpy saved(&mgr, &SessionManager::sessionSaved);
    mgr.saveSession(); // triggers saveUnsavedBuffers internally
    QCOMPARE(saved.count(), 1);

    QSignalSpy hotExit(&mgr, &SessionManager::hotExitFileRequested);
    bool ok = mgr.restoreSession();
    QVERIFY(ok);
    QVERIFY(hotExit.count() >= 1);
    QCOMPARE(hotExit.first().at(1).toString(), QString("unsaved content"));
}

void TestSessionManager::testHotExitNoData()
{
    QMainWindow win;
    QTabWidget tabs;
    CodeEditor editor;
    editor.setFilePath("/does/not/exist.txt"); // clean file, not modified
    editor.setPlainText("clean");
    tabs.addTab(&editor, "x");

    SessionManager mgr(&win, &tabs);
    mgr.clearSession(); // ensure no leftover hot-exit data
    QVERIFY(!mgr.restoreUnsavedBuffers());
}

void TestSessionManager::testAutoSaveToggle()
{
    QMainWindow win;
    QTabWidget tabs;
    SessionManager mgr(&win, &tabs);
    mgr.setAutoSaveSession(false);
    QVERIFY(!mgr.autoSaveSession());
    mgr.setAutoSaveSession(true);
    QVERIFY(mgr.autoSaveSession());

    mgr.setHotExitEnabled(false);
    QVERIFY(!mgr.hotExitEnabled());
    mgr.setHotExitEnabled(true);
    QVERIFY(mgr.hotExitEnabled());
}
