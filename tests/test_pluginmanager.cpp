#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include "pluginmanager.h"
#include "versionfetcher.h"
#include "test_pluginmanager.h"

static void writeJson(const QString &dir, const QString &fileName, const QJsonObject &obj)
{
    QDir().mkpath(dir);
    QFile f(dir + "/" + fileName);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(obj).toJson());
    f.close();
}

void TestPluginManager::testLoadPluginMetadataValid()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeJson(dir.path(), "plugin.json",
              QJsonObject{
                  {"id", "com.test.demo"},
                  {"name", "Demo"},
                  {"version", "1.2.3"},
                  {"library", "libdemo.so"},
                  {"dependencies", QJsonArray{"com.test.core"}},
              });

    PluginManager pm;
    QJsonObject meta;
    QVERIFY(pm.loadPluginMetadata(dir.path(), meta));
    QCOMPARE(meta["id"].toString(), QString("com.test.demo"));
    QCOMPARE(meta["version"].toString(), QString("1.2.3"));
}

void TestPluginManager::testLoadPluginMetadataMissingFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PluginManager pm;
    QJsonObject meta;
    QVERIFY(!pm.loadPluginMetadata(dir.path(), meta));
}

void TestPluginManager::testLoadPluginMetadataInvalidJson()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile f(dir.path() + "/plugin.json");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("{not valid json");
    f.close();

    PluginManager pm;
    QJsonObject meta;
    QVERIFY(!pm.loadPluginMetadata(dir.path(), meta));
}

void TestPluginManager::testCheckVersionCompatibilityNoConstraint()
{
    PluginManager pm;
    QJsonObject meta; // no incompatible_with key
    QVERIFY(pm.checkVersionCompatibility(meta));
}

void TestPluginManager::testCheckVersionCompatibilityNonMatching()
{
    PluginManager pm;
    QJsonObject meta;
    meta["incompatible_with"] = QJsonArray{"0.0.1", "9.9.9"};
    QVERIFY(pm.checkVersionCompatibility(meta));
}

void TestPluginManager::testCheckVersionCompatibilityMatching()
{
    PluginManager pm;
    QJsonObject meta;
    meta["incompatible_with"] = QJsonArray{VersionFetcher::coreVersion()};
    QVERIFY(!pm.checkVersionCompatibility(meta));
}

void TestPluginManager::testBuildDependencyGraphAndSort()
{
    PluginManager pm;
    QList<QJsonObject> plugins;
    plugins.append(QJsonObject{{"id", "app"}, {"dependencies", QJsonArray{"core"}}});
    plugins.append(QJsonObject{{"id", "core"}, {"dependencies", QJsonArray{}}});

    QVERIFY(pm.buildDependencyGraph(plugins));
    QStringList order = pm.topologicalSort();
    QVERIFY(order.contains("app"));
    QVERIFY(order.contains("core"));
    QVERIFY(order.indexOf("core") < order.indexOf("app"));
}

void TestPluginManager::testDisabledPluginsPersistence()
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configPath);
    QString filePath = configPath + "/disabled_plugins.json";

    // Seed a disabled-plugin list and let the manager load it.
    QJsonArray arr;
    arr.append("com.test.disabled");
    arr.append("com.test.other");
    QFile seed(filePath);
    QVERIFY(seed.open(QIODevice::WriteOnly));
    seed.write(QJsonDocument(arr).toJson());
    seed.close();

    PluginManager pm;
    pm.loadDisabledPlugins();
    QVERIFY(pm.isDisabled("com.test.disabled"));
    QVERIFY(pm.isDisabled("com.test.other"));
    QVERIFY(pm.disabledPlugins().contains("com.test.disabled"));

    // Reload from disk to confirm persistence survived a fresh instance.
    PluginManager pm2;
    pm2.loadDisabledPlugins();
    QVERIFY(pm2.isDisabled("com.test.disabled"));

    QFile::remove(filePath);
}
