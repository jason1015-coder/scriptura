#include <QTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QRandomGenerator>
#include "thememanager.h"
#include "test_thememanager.h"
#include "rust_adapter.h"

void TestThemeManager::testDefaultTheme()
{
    ThemeManager mgr;
    auto theme = mgr.currentTheme();
    QCOMPARE(theme.family, ThemeManager::ColorFamily::Default);
    QCOMPARE(theme.mode, ThemeManager::Mode::Light);
    QVERIFY(!theme.isDark());
}

void TestThemeManager::testApplyTheme()
{
    ThemeManager mgr;
    ThemeManager::Theme dark(ThemeManager::ColorFamily::Default, ThemeManager::Mode::Dark);
    mgr.applyTheme(dark);
    auto current = mgr.currentTheme();
    QVERIFY(current.isDark());
    QCOMPARE(current.mode, ThemeManager::Mode::Dark);
}

void TestThemeManager::testThemeProperties()
{
    ThemeManager mgr;
    QColor accent = mgr.accentColor();
    QColor bg = mgr.backgroundColor();
    QColor text = mgr.textColor();
    QColor border = mgr.borderColor();
    
    // Colors should be valid
    QVERIFY(accent.isValid());
    QVERIFY(bg.isValid());
    QVERIFY(text.isValid());
    QVERIFY(border.isValid());
    
    // Generated stylesheet should be non-empty
    QString stylesheet = mgr.generateGlobalStylesheet();
    QVERIFY(!stylesheet.isEmpty());
    
    // Design tokens should be non-empty
    QString tokens = mgr.generateDesignTokens();
    QVERIFY(!tokens.isEmpty());
}

void TestThemeManager::testPluginResolution()
{
    // --- Phase 1: Setup (fixture a plugin theme in a temporary dir) ---
    const QString pluginDir = QDir::tempPath() + "/scriptura_test_plugin_" + QString::number(QRandomGenerator::global()->bounded(1'000'000));
    QDir().mkpath(pluginDir);
    
    // Create a minimal theme plugin JSON file that merges with the default theme
    // Format: <id>.json with colors for the theme kind.
    const QString pluginId = "test_theme_plugin";
    const QString pluginFile = pluginDir + "/" + pluginId + ".json";
    QJsonObject pluginObj;
    pluginObj["id"] = pluginId;
    pluginObj["name"] = "Test Theme Plugin";
    pluginObj["version"] = "1.0.0";
    pluginObj["kind"] = "theme";
    pluginObj["state"] = "active";
    pluginObj["enabled"] = true;
    QJsonObject themeConfig;
    QJsonObject themeColors;
    themeColors["bgColor"] = "#200022";        // Dark purple background (overrides base)
    themeColors["textColor"] = "#FFFFFF";      // White text (overrides base)
    themeColors["highlightColor"] = "#FF00FF"; // Custom magenta accent (overrides base)
    themeConfig["colors"] = themeColors;
    pluginObj["theme"] = themeConfig;
    
    QFile f(pluginFile);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QVERIFY(f.write(QJsonDocument(pluginObj).toJson()));
    f.close();
    
    // --- Phase 2: Use the RustBackend adapter to scan and resolve ---
    auto *registry = RustBackend::instance()->pluginRegistry();
    registry->scan(pluginDir);

    // Resolved theme ids use the documented "plugin-id:theme-id" format, so a
    // legacy single-theme file resolves under "<plugin-id>:<plugin-id>"
    // (see README → Theme plugins).
    const QString resolvedId = pluginId + ":" + pluginId;
    QJsonObject resolved = registry->resolved(resolvedId).toObject();
    QVERIFY(!resolved.isEmpty()); // Should resolve to a complete ThemeDefinition
    
    // Validate the merged colors: plugin-specific keys override the built-in base.
    QCOMPARE(resolved["highlightColor"].toString(), "#FF00FF");
    QCOMPARE(resolved["bgColor"].toString(), "#200022");
    QCOMPARE(resolved["textColor"].toString(), "#FFFFFF");
    
    // --- Phase 3: Use ThemeManager's custom-theme path (Qt-side integration) ---
    // applyCustomTheme(pluginsDir, id) resolves via the registry using the given
    // directory and applies the resolved ThemeDefinition. Passing our temp dir
    // keeps the test hermetic (the default plugins dir is not consulted here).
    ThemeManager tm;
    QVERIFY(tm.applyCustomTheme(pluginDir, resolvedId));

    // The registry (single source of truth) must list the theme as an enabled
    // theme plugin under its compound id. We drive this off the same temp dir so
    // it stays hermetic.
    QStringList enabledThemeIds;
    const QJsonArray allPlugins = registry->list();
    for (const QJsonValue &v : allPlugins) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("kind")).toString() == QLatin1String("theme")
            && o.value(QStringLiteral("enabled")).toBool()) {
            enabledThemeIds << o.value(QStringLiteral("id")).toString();
        }
    }
    // The enabled check uses the base ID: list() now aggregates one plugin
    // file to one row, exposing the base ID rather than compound theme IDs.
    QVERIFY(enabledThemeIds.contains(pluginId));

    // --- Phase 4: Cleanup ---
    QDir(pluginDir).removeRecursively();
}

// Validates the registry signal wiring the Plugins UI relies on: an Enable
// toggle persists the disabled flag in Rust, emits registryChanged(), and the
// disabled theme then disappears from the Appearance → Themes (enabled) set.
void TestThemeManager::testPluginEnableToggle()
{
    auto *registry = RustBackend::instance()->pluginRegistry();

    // Fixture two theme plugins in a temp dir. Ids are unique per run so the
    // persisted disabled flag (keyed by id) never collides across runs.
    const QString suffix = QString::number(QRandomGenerator::global()->bounded(1'000'000));
    const QString pluginDir = QDir::tempPath() + "/scriptura_test_toggle_" + suffix;
    QDir().mkpath(pluginDir);
    const QString idA = "theme_a_" + suffix;
    const QString idB = "theme_b_" + suffix;
    // The registry exposes theme plugins under compound ids internally, but
    // list() now aggregates to base ids (one plugin file = one row) and the
    // UI toggles by base id. resolved() still uses compound ids for theme
    // application (README → Theme plugins).

    auto writePlugin = [&](const QString &id, const QString &bg) {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = id;
        obj["version"] = "1.0.0";
        obj["kind"] = "theme";
        obj["state"] = "active";
        QJsonObject theme;
        QJsonObject colors;
        colors["bgColor"] = bg;
        theme["colors"] = colors;
        obj["theme"] = theme;
        QFile f(pluginDir + "/" + id + ".json");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(obj).toJson());
        f.close();
    };
    writePlugin(idA, "#111111");
    writePlugin(idB, "#222222");

    // Spy on the signal the UI connects to.
    QSignalSpy spy(registry, &RustPluginRegistryAdapter::registryChanged);

    registry->scan(pluginDir);
    QCOMPARE(spy.count(), 1); // scan() emits once

    auto enabledThemeIds = [registry]() {
        QStringList ids;
        for (const QJsonValue &v : registry->list()) {
            const QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("kind")).toString() == QLatin1String("theme")
                && o.value(QStringLiteral("enabled")).toBool()) {
                ids << o.value(QStringLiteral("id")).toString();
            }
        }
        return ids;
    };

    // Both themes start enabled and appear in the Appearance set.
    // list() returns base ids (one plugin file = one row).
    QStringList enabled = enabledThemeIds();
    QVERIFY(enabled.contains(idA));
    QVERIFY(enabled.contains(idB));

    // Disabling theme_a must emit registryChanged and drop it from Appearance.
    // The UI passes the base id from list(); set_disabled updates all
    // compound records with the matching prefix.
    registry->setDisabled(idA, true);
    QCOMPARE(spy.count(), 2);
    enabled = enabledThemeIds();
    QVERIFY(!enabled.contains(idA));
    QVERIFY(enabled.contains(idB));

    // Re-enabling brings it back.
    registry->setDisabled(idA, false);
    QCOMPARE(spy.count(), 3);
    enabled = enabledThemeIds();
    QVERIFY(enabled.contains(idA));

    QDir(pluginDir).removeRecursively();
}
