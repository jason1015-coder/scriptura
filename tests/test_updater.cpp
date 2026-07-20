#include <QTest>
#include <QSignalSpy>
#include "updater.h"
#include "test_updater.h"

void TestUpdater::testInitialState()
{
    Updater updater;
    QVERIFY(updater.isUpdateCheckEnabled());
    QVERIFY(updater.latestVersion().isEmpty());
    QVERIFY(updater.downloadUrl().isEmpty());
    QCOMPARE(updater.lastCheckedType(), Updater::Stable);
}

void TestUpdater::testSetCheckInterval()
{
    Updater updater;
    updater.setUpdateCheckInterval(14);
    QCOMPARE(updater.lastCheckedType(), Updater::Stable);
}

void TestUpdater::testCheckEnabled()
{
    Updater updater;
    QVERIFY(updater.isUpdateCheckEnabled());
    updater.setUpdateCheckEnabled(false);
    QVERIFY(!updater.isUpdateCheckEnabled());
    updater.setUpdateCheckEnabled(true);
    QVERIFY(updater.isUpdateCheckEnabled());
}
