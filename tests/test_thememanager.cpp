#include <QTest>
#include "thememanager.h"
#include "test_thememanager.h"

void TestThemeManager::testDefaultTheme()
{
    ThemeManager mgr;
    auto theme = mgr.currentTheme();
    QCOMPARE(theme.family, ThemeManager::ColorFamily::Default);
    QCOMPARE(theme.mode, ThemeManager::Mode::Light);
    QVERIFY(!theme.isDark());
}

void TestThemeManager::testApplyTheme()
{
    ThemeManager mgr;
    ThemeManager::Theme dark(ThemeManager::ColorFamily::Default, ThemeManager::Mode::Dark);
    mgr.applyTheme(dark);
    auto current = mgr.currentTheme();
    QVERIFY(current.isDark());
    QCOMPARE(current.mode, ThemeManager::Mode::Dark);
}

void TestThemeManager::testThemeProperties()
{
    ThemeManager mgr;
    QColor accent = mgr.accentColor();
    QColor bg = mgr.backgroundColor();
    QColor text = mgr.textColor();
    QColor border = mgr.borderColor();
    
    // Colors should be valid
    QVERIFY(accent.isValid());
    QVERIFY(bg.isValid());
    QVERIFY(text.isValid());
    QVERIFY(border.isValid());
    
    // Generated stylesheet should be non-empty
    QString stylesheet = mgr.generateGlobalStylesheet();
    QVERIFY(!stylesheet.isEmpty());
    
    // Design tokens should be non-empty
    QString tokens = mgr.generateDesignTokens();
    QVERIFY(!tokens.isEmpty());
}
