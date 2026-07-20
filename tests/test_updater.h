#ifndef TEST_UPDATER_H
#define TEST_UPDATER_H

#include <QObject>

class TestUpdater : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testSetCheckInterval();
    void testCheckEnabled();
};

#endif
