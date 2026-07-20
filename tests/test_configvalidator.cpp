#include <QTest>
#include <QFile>
#include <QSettings>
#include <QJsonObject>
#include <QStandardPaths>
#include "configvalidator.h"
#include "test_configvalidator.h"

void TestConfigValidator::initTestCase()
{
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QCoreApplication::setOrganizationName("ScripturaTests");
    QCoreApplication::setApplicationName("ConfigValidator");

    // Qt 6: QSettings::setPath() is deprecated and has no effect.
    // Use QStandardPaths test mode to redirect settings to a temp location
    // instead of polluting the real user config (registry on Windows).
    QStandardPaths::setTestModeEnabled(true);
}

void TestConfigValidator::cleanupTestCase()
{
    // Restore normal mode so subsequent test classes use real paths
    QStandardPaths::setTestModeEnabled(false);
}

void TestConfigValidator::testValidTabWidthAccepted()
{
    ConfigValidator validator;

    QSettings settings;
    settings.setValue("editor/tabWidth", 4);
    settings.sync();

    QStringList invalid = validator.validateSettings();
    QVERIFY(!invalid.contains("editor/tabWidth"));
}

void TestConfigValidator::testInvalidTabWidthRejected()
{
    ConfigValidator validator;

    QSettings settings;
    settings.setValue("editor/tabWidth", 99);
    settings.sync();

    QStringList invalid = validator.validateSettings();
    QVERIFY(invalid.contains("editor/tabWidth"));
}

void TestConfigValidator::testValidBooleanAccepted()
{
    ConfigValidator validator;

    QSettings settings;
    settings.setValue("ui/sidebarCollapsed", true);
    settings.sync();

    QStringList invalid = validator.validateSettings();
    QVERIFY(!invalid.contains("ui/sidebarCollapsed"));
}

void TestConfigValidator::testValidVersionStringAccepted()
{
    ConfigValidator validator;

    QSettings settings;
    settings.setValue("updater/currentVersion", "1.2.3");
    settings.sync();

    QStringList invalid = validator.validateSettings();
    QVERIFY(!invalid.contains("updater/currentVersion"));
}

void TestConfigValidator::testValidateSettingsReturnsEmptyOnCleanSettings()
{
    ConfigValidator validator;

    QSettings settings;
    settings.clear();
    settings.sync();

    QStringList invalid = validator.validateSettings();
    QCOMPARE(invalid, QStringList());
}

void TestConfigValidator::testResetInvalidSettingsRestoresDefaults()
{
    ConfigValidator validator;

    QSettings settings;
    settings.setValue("editor/tabWidth", 99);
    settings.setValue("updater/checkInterval", 999);
    settings.sync();

    validator.resetInvalidSettings();

    QCOMPARE(settings.value("editor/tabWidth").toInt(), 4);
    QCOMPARE(settings.value("updater/checkInterval").toInt(), 7);
}

void TestConfigValidator::testGetValidatedValueReturnsDefaultForInvalid()
{
    ConfigValidator validator;

    QSettings settings;
    settings.setValue("editor/tabWidth", 99);
    settings.sync();

    int val = validator.getValidatedValue<int>("editor/tabWidth", 4);
    QCOMPARE(val, 4);
}
