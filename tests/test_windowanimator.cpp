#include <QTest>
#include <QWidget>
#include "windowanimator.h"
#include "test_windowanimator.h"

void TestWindowAnimator::testInitialState()
{
    WindowAnimator animator;
    // Default constructed - just verify it doesn't crash
    QVERIFY(true);
}

void TestWindowAnimator::testStopAllAnimations()
{
    WindowAnimator animator;
    // stopAllAnimations with no running animations should be a no-op
    animator.stopAllAnimations();
    QVERIFY(true);
}
