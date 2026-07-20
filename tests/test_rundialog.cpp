#include <QTest>
#include "rundialog.h"
#include "test_rundialog.h"

void TestRunDialog::testInitialState()
{
    RunDialog dialog;
    QVERIFY(dialog.configurations().isEmpty());
    QCOMPARE(dialog.selectedMode(), QString("run"));
}

void TestRunDialog::testSetConfigurations()
{
    RunDialog dialog;
    QList<DebugConfiguration> configs;
    DebugConfiguration config;
    config.name = "Test App";
    config.type = "cppdbg";
    config.request = "launch";
    config.program = "/usr/bin/test";
    config.args = {"--verbose"};
    config.cwd = "/home/user";
    configs.append(config);
    
    dialog.setConfigurations(configs);
    QCOMPARE(dialog.configurations().size(), 1);
    QCOMPARE(dialog.configurations().first().name, QString("Test App"));
    
    auto selected = dialog.selectedConfiguration();
    QCOMPARE(selected.name, QString("Test App"));
}

void TestRunDialog::testEmptyConfigurations()
{
    RunDialog dialog;
    dialog.setConfigurations({});
    QVERIFY(dialog.configurations().isEmpty());
}
