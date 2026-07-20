#include <QTest>
#include <QFile>
#include <QDir>
#include "crashhandler.h"
#include "test_crashhandler.h"

void TestCrashHandler::testInstall()
{
    // install() sets signal handlers and/or exception filters.
    // We cannot easily test the platform-specific signal handling, but
    // we can verify the function doesn't crash.
    CrashHandler::install();
    QVERIFY(true);  // reached without crash
}

void TestCrashHandler::testDumpPath()
{
    QString path = CrashHandler::dumpPath();
    QVERIFY(!path.isEmpty());
    // Must be an absolute path to a temp-like location
    QVERIFY(QDir::isAbsolutePath(path));
}
