#ifndef TEST_PLUGINMANAGER_H
#define TEST_PLUGINMANAGER_H

#include <QObject>

class TestPluginManager : public QObject
{
    Q_OBJECT
private slots:
    void testLoadPluginMetadataValid();
    void testLoadPluginMetadataMissingFile();
    void testLoadPluginMetadataInvalidJson();
    void testCheckVersionCompatibilityNoConstraint();
    void testCheckVersionCompatibilityNonMatching();
    void testCheckVersionCompatibilityMatching();
    void testBuildDependencyGraphAndSort();
    void testDisabledPluginsPersistence();
};

#endif // TEST_PLUGINMANAGER_H
