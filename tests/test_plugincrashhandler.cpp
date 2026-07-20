#include <QTest>
#include <QSignalSpy>
#include "plugincrashhandler.h"
#include "test_plugincrashhandler.h"

void TestPluginCrashHandler::testInitialState()
{
    PluginCrashHandler handler;
    QVERIFY(!handler.isPluginDisabled("com.test.plugin"));
    QVERIFY(handler.recentCrashes().isEmpty());
}

void TestPluginCrashHandler::testHandleCrash()
{
    PluginCrashHandler handler;
    QSignalSpy spy(&handler, &PluginCrashHandler::pluginCrashed);

    handler.handleCrash("com.test.plugin");
    QCOMPARE(spy.count(), 1);
    QVERIFY(handler.isPluginDisabled("com.test.plugin"));
    QCOMPARE(handler.recentCrashes().size(), 1);
    QCOMPARE(handler.recentCrashes().first().pluginId, QString("com.test.plugin"));
}

void TestPluginCrashHandler::testDisableAndEnable()
{
    PluginCrashHandler handler;
    handler.disablePlugin("com.test.plugin");
    QVERIFY(handler.isPluginDisabled("com.test.plugin"));

    handler.enablePlugin("com.test.plugin");
    QVERIFY(!handler.isPluginDisabled("com.test.plugin"));
}

void TestPluginCrashHandler::testRecentCrashesLimit()
{
    PluginCrashHandler handler;
    for (int i = 0; i < 5; ++i)
        handler.handleCrash(QString("plugin%1").arg(i));

    QCOMPARE(handler.recentCrashes().size(), 5);
    QCOMPARE(handler.recentCrashes(3).size(), 3);
    QCOMPARE(handler.recentCrashes(10).size(), 5);
}
