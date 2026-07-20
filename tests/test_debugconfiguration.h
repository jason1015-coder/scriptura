#ifndef TEST_DEBUGCONFIGURATION_H
#define TEST_DEBUGCONFIGURATION_H

#include <QObject>

class TestDebugConfiguration : public QObject
{
    Q_OBJECT
private slots:
    void testDefaultConfig();
    void testConfigManager();
};

#endif
