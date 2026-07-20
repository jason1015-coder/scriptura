#include <QTest>
#include "debugconfiguration.h"
#include "test_debugconfiguration.h"

void TestDebugConfiguration::testDefaultConfig()
{
    DebugConfiguration config;
    QVERIFY(config.name.isEmpty());
    QVERIFY(config.program.isEmpty());
    QVERIFY(config.args.isEmpty());
    QVERIFY(config.cwd.isEmpty());
}

void TestDebugConfiguration::testConfigManager()
{
    DebugConfigurationManager manager;
    QVERIFY(manager.configurations().isEmpty());

    DebugConfiguration config;
    config.name = QStringLiteral("Test Config");
    config.program = QStringLiteral("/usr/bin/test");
    config.args = QStringList(QStringLiteral("--test"));
    config.cwd = QStringLiteral("/tmp");
    manager.addConfiguration(config);
    QCOMPARE(manager.configurations().size(), 1);

    manager.removeConfiguration(QStringLiteral("Test Config"));
    QVERIFY(manager.configurations().isEmpty());
}
