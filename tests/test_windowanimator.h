#ifndef TEST_WINDOWANIMATOR_H
#define TEST_WINDOWANIMATOR_H

#include <QObject>

class TestWindowAnimator : public QObject
{
    Q_OBJECT
private slots:
    void testInitialState();
    void testStopAllAnimations();
};

#endif
