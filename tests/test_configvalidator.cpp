#include <QtTest/QtTest>
#include "configvalidator.h"

class TestConfigValidator : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
        // Set up test environment
    }

    void cleanupTestCase() {
        // Clean up
    }

    void testValidateSettings_data() {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QVariant>("value");
        QTest::addColumn<bool>("expectedValid");

        QTest::newRow("valid theme") << "theme/selected" << QVariant(0) << true;
        QTest::newRow("invalid theme negative") << "theme/selected" << QVariant(-1) << false;
        QTest::newRow("invalid theme too large") << "theme/selected" << QVariant(101) << false;
        QTest::newRow("valid tab width") << "editor/tabWidth" << QVariant(4) << true;
        QTest::newRow("invalid tab width") << "editor/tabWidth" << QVariant(0) << false;
        QTest::newRow("valid sidebar collapsed") << "ui/sidebarCollapsed" << QVariant(true) << true;
    }

    void testValidateSettings() {
        QFETCH(QString, key);
        QFETCH(QVariant, value);
        QFETCH(bool, expectedValid);

        QSettings settings;
        settings.setValue(key, value);

        ConfigValidator validator;
        QStringList invalidKeys = validator.validateSettings();

        if (expectedValid) {
            QVERIFY(!invalidKeys.contains(key));
        } else {
            QVERIFY(invalidKeys.contains(key));
        }
    }

    void testGetValidatedValue() {
        QSettings settings;
        settings.setValue("editor/tabWidth", 8);

        ConfigValidator validator;
        int tabWidth = validator.getValidatedValue<int>("editor/tabWidth", 4);
        QCOMPARE(tabWidth, 8);

        // Test with invalid value - should return default
        settings.setValue("editor/tabWidth", 100); // Invalid
        int defaultTabWidth = validator.getValidatedValue<int>("editor/tabWidth", 4);
        QCOMPARE(defaultTabWidth, 4);
    }
};

QTEST_MAIN(TestConfigValidator)
#include "test_configvalidator.moc"