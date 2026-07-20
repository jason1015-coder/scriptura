#ifndef TEST_PLUGINCRASHHANDLER_H
#define TEST_PLUGINCRASHHANDLER_H

#include <QObject>

class TestPluginCrashHandler : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testHandleCrash();
    void testDisableAndEnable();
    void testRecentCrashesLimit();
};

#endif
