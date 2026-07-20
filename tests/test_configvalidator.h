#ifndef TEST_CONFIGVALIDATOR_H
#define TEST_CONFIGVALIDATOR_H

#include <QObject>

class TestConfigValidator : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void testValidTabWidthAccepted();
    void testInvalidTabWidthRejected();
    void testValidBooleanAccepted();
    void testValidVersionStringAccepted();
    void testValidateSettingsReturnsEmptyOnCleanSettings();
    void testResetInvalidSettingsRestoresDefaults();
    void testGetValidatedValueReturnsDefaultForInvalid();
};

#endif // TEST_CONFIGVALIDATOR_H
