#ifndef TEST_APPLICATIONDOCK_H
#define TEST_APPLICATIONDOCK_H

#include <QObject>

class TestApplicationDock : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testAddRemoveEntry();
    void testSetActiveApp();
    void testUpdateTheme();
    void testThemeAnimation();
    void testSetThemeManager();
};

#endif // TEST_APPLICATIONDOCK_H
