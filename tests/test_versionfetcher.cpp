#include <QTest>
#include <QSignalSpy>
#include "versionfetcher.h"
#include "test_versionfetcher.h"

void TestVersionFetcher::testCoreVersion()
{
    QString version = VersionFetcher::coreVersion();
    QVERIFY(!version.isEmpty());
    // Version should be a non-empty string (e.g. "0.2.0" or "0.0.0-dev")
    QVERIFY(version.contains('.'));
}
