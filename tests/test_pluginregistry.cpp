#include <QTest>
#include <QSignalSpy>
#include <QUrl>
#include "pluginregistry.h"
#include "test_pluginregistry.h"

void TestPluginRegistry::testInitialState()
{
    PluginRegistry reg;
    QVERIFY(reg.manifest().isEmpty());
    QVERIFY(reg.registryUrl().isEmpty());
}

void TestPluginRegistry::testSetRegistryUrl()
{
    PluginRegistry reg;
    QUrl url("https://example.com/registry.json");
    reg.setRegistryUrl(url);
    QCOMPARE(reg.registryUrl(), url);
}
