#ifndef TEST_PLUGINSETTINGS_H
#define TEST_PLUGINSETTINGS_H

#include <QObject>

class TestPluginSettings : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void testValueReturnsDefault();
    void testSetAndGetValue();
    void testSetDefaults();
    void testResetToDefaults();
    void testTypedValueAccess();
    void testOwnsSettings();
};

#endif // TEST_PLUGINSETTINGS_H
