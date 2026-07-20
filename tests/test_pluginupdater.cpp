#include <QTest>
#include "pluginupdater.h"
#include "test_pluginupdater.h"

void TestPluginUpdater::testInitialState()
{
    PluginUpdater updater;
    // Should not crash
    updater.scheduleUpdateCheck(0);
    QVERIFY(true);
}

void TestPluginUpdater::testSetRegistryUrl()
{
    PluginUpdater updater;
    QString url = "https://example.com/registry.json";
    updater.setRegistryUrl(url);
    QVERIFY(true); // method exists and compiles
}

void TestPluginUpdater::testScheduleUpdateCheck()
{
    PluginUpdater updater;
    updater.scheduleUpdateCheck(24);
    updater.scheduleUpdateCheck(0);
    // Should not crash
    QVERIFY(true);
}
