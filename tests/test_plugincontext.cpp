#include <QTest>
#include <QSettings>
#include <QCoreApplication>
#include "plugincontext.h"
#include "permission.h"
#include "rust_adapter.h"
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
    // The Rust permission manager denies by default (nothing is granted),
    // so an unknown plugin has no permissions.
    QVERIFY(!ctx.hasPermission("test-plugin", Permission::FileRead));

    // After granting a permission, the same check passes.
    RustBackend::instance()->permissionManager()->grantPermission(
        "test-plugin", Permission::FileRead);
    QVERIFY(ctx.hasPermission("test-plugin", Permission::FileRead));

    // A different (non-granted) permission is still denied.
    QVERIFY(!ctx.hasPermission("test-plugin", Permission::NetworkAccess));

    // requestPermission must not crash.
    ctx.requestPermission("test-plugin", Permission::NetworkAccess);
    QVERIFY(true);
}
