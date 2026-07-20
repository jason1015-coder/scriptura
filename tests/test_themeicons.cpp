#include <QTest>
#include "themeicons.h"
#include "test_themeicons.h"

void TestThemeIcons::testRoles()
{
    QVERIFY(static_cast<int>(ThemeIcons::Role::Normal) == 0);
    QVERIFY(static_cast<int>(ThemeIcons::Role::Selected) == 1);
    QVERIFY(static_cast<int>(ThemeIcons::Role::Disabled) == 2);
    QVERIFY(static_cast<int>(ThemeIcons::Role::Accent) == 3);
}

void TestThemeIcons::testSingletonInstance()
{
    ThemeIcons *ti = ThemeIcons::instance();
    QVERIFY(ti != nullptr);
    // Calling twice should return the same instance
    ThemeIcons *ti2 = ThemeIcons::instance();
    QVERIFY(ti == ti2);
}

void TestThemeIcons::testRecolorAll()
{
    ThemeIcons *ti = ThemeIcons::instance();
    QVERIFY(ti != nullptr);
    // recolorAll should not crash even with no registered icons
    ti->recolorAll();
    QVERIFY(true);
}
