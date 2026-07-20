#ifndef TEST_PLUGINUPDATER_H
#define TEST_PLUGINUPDATER_H

#include <QObject>

class TestPluginUpdater : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testSetRegistryUrl();
    void testScheduleUpdateCheck();
};

#endif
