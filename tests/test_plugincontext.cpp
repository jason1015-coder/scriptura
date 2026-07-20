#include <QTest>
#include <QSettings>
#include <QCoreApplication>
#include "plugincontext.h"
#include "permission.h"
#include "test_plugincontext.h"

void TestPluginContext::testCreateWithoutMainWindow()
{
    PluginContext ctx(nullptr);
    QVERIFY(ctx.mainWindow() == nullptr);
    QVERIFY(ctx.settings() == nullptr);
    QVERIFY(ctx.currentEditor() == nullptr);
    QVERIFY(ctx.currentProjectPath().isEmpty());
}

void TestPluginContext::testPermissionCheck()
{
    PluginContext ctx(nullptr);
    // Without permission manager, all checks pass
    QVERIFY(ctx.hasPermission("test-plugin", Permission::FileRead));
}
