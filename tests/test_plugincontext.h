#ifndef TEST_PLUGINCONTEXT_H
#define TEST_PLUGINCONTEXT_H

#include <QObject>

class TestPluginContext : public QObject
{
    Q_OBJECT
private slots:
    void testCreateWithoutMainWindow();
    void testPermissionCheck();
};

#endif
