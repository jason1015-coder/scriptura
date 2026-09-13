#include <QTest>
#include <QFile>
#include <QDir>
#include "rust_backend.h"
#include "test_crashhandler.h"

void TestCrashHandler::testInstall()
{
    RustAppCrashHandler *h = rust_app_crash_new();
    rust_app_crash_install(h);
    rust_app_crash_free(h);
    QVERIFY(true);
}

void TestCrashHandler::testDumpPath()
{
    RustAppCrashHandler *h = rust_app_crash_new();
    char *path = rust_app_crash_dump_path(h);
    QString pathStr = QString::fromUtf8(path);
    rust_free_string(path);
    rust_app_crash_free(h);
    QVERIFY(!pathStr.isEmpty());
    QVERIFY(QDir::isAbsolutePath(pathStr));
}
