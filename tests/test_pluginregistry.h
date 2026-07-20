#ifndef TEST_PLUGINREGISTRY_H
#define TEST_PLUGINREGISTRY_H

#include <QObject>

class TestPluginRegistry : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testSetRegistryUrl();
};

#endif
