#include <QTest>
#include "applicationdock.h"
#include "thememanager.h"
#include "test_applicationdock.h"

// ── Tests for initial state ───────────────────────────────────────────

void TestApplicationDock::testInitialState()
{
    ApplicationDock dock;
    auto colors = dock.currentThemeColors();
    // Default state should be dark-themed
    QVERIFY(colors.isDark);
    QVERIFY(colors.glassBg.isValid());
    QVERIFY(colors.glassBgEnd.isValid());
    QVERIFY(colors.borderColor.isValid());
    QVERIFY(colors.accentColor.isValid());

    // Dock should be empty initially
    QCOMPARE(dock.entryCount(), 0);
    QVERIFY(!dock.hasEntry("anything"));
}

// ── Tests for entry management ────────────────────────────────────────

void TestApplicationDock::testAddRemoveEntry()
{
    ApplicationDock dock;

    int idx = dock.addEntry("test-app", ":/icons/tools.svg", "Test App");
    QVERIFY(idx >= 0);
    QCOMPARE(dock.entryCount(), 1);
    QVERIFY(dock.hasEntry("test-app"));

    // Duplicate add should return -1
    int dupIdx = dock.addEntry("test-app", ":/icons/tools.svg", "Test App");
    QCOMPARE(dupIdx, -1);
    QCOMPARE(dock.entryCount(), 1);

    // Add a second entry
    dock.addEntry("app2", ":/icons/git.svg", "App 2");
    QCOMPARE(dock.entryCount(), 2);

    // Remove
    dock.removeEntry("test-app");
    QCOMPARE(dock.entryCount(), 1);
    QVERIFY(!dock.hasEntry("test-app"));
    QVERIFY(dock.hasEntry("app2"));

    // Remove non-existent should be a no-op
    dock.removeEntry("non-existent");
    QCOMPARE(dock.entryCount(), 1);
}

// ── Tests for active app tracking ─────────────────────────────────────

void TestApplicationDock::testSetActiveApp()
{
    ApplicationDock dock;
    dock.addEntry("app1", ":/icons/tools.svg", "App 1");
    dock.addEntry("app2", ":/icons/git.svg", "App 2");

    // Initially no active app — colors should be defaults
    auto before = dock.currentThemeColors();
    Q_UNUSED(before)

    dock.setActiveApp("app1");
    // Active state is set; verify it doesn't crash or change theme colors
    auto after = dock.currentThemeColors();
    QVERIFY(after.accentColor.isValid());

    // Switching to another app
    dock.setActiveApp("app2");
    auto switched = dock.currentThemeColors();
    QCOMPARE(switched.accentColor, after.accentColor);

    // Clearing active app
    dock.setActiveApp(QString());
}

// ── Tests for updateTheme (direct color change + animation) ───────────

void TestApplicationDock::testUpdateTheme()
{
    ApplicationDock dock;

    // Apply a light theme
    QColor lightBg(240, 240, 245, 230);
    QColor lightBorder(0, 0, 0, 25);
    QColor accent(0, 122, 255);
    dock.updateTheme(lightBg, lightBorder, accent, false);

    // The animation is started; force it to finish by advancing to end value
    // We can do this by processing events and advancing the animation
    QTest::qWait(350); // Wait for the 300ms animation to finish + buffer

    auto colors = dock.currentThemeColors();
    QVERIFY(!colors.isDark);
    QCOMPARE(colors.glassBg, lightBg);
    QCOMPARE(colors.borderColor, lightBorder);
    QCOMPARE(colors.accentColor, accent);

    // Now apply a dark theme
    QColor darkBg(56, 58, 61, 230);
    QColor darkBorder(255, 255, 255, 20);
    QColor darkAccent(100, 150, 255);
    dock.updateTheme(darkBg, darkBorder, darkAccent, true);

    QTest::qWait(350); // Wait for animation to finish

    auto darkColors = dock.currentThemeColors();
    QVERIFY(darkColors.isDark);
    QCOMPARE(darkColors.glassBg, darkBg);
    QCOMPARE(darkColors.borderColor, darkBorder);
    QCOMPARE(darkColors.accentColor, darkAccent);
}

// ── Tests that animation actually interpolates ────────────────────────

void TestApplicationDock::testThemeAnimation()
{
    ApplicationDock dock;

    // Start with known colors (dark)
    QColor darkBg(56, 58, 61, 230);
    QColor darkBorder(255, 255, 255, 20);
    QColor darkAccent(100, 150, 255);
    dock.updateTheme(darkBg, darkBorder, darkAccent, true);
    QTest::qWait(350);

    auto before = dock.currentThemeColors();
    QCOMPARE(before.glassBg, darkBg);

    // Update to light colors — check that animation is in progress
    QColor lightBg(240, 240, 245, 230);
    QColor lightBorder(0, 0, 0, 25);
    QColor lightAccent(0, 122, 255);
    dock.updateTheme(lightBg, lightBorder, lightAccent, false);

    // Check mid-animation (after a short delay, colors should have started moving toward target)
    QTest::qWait(50); // Let animation run for 50ms
    auto mid = dock.currentThemeColors();
    // Mid colors should not equal the final target (animation hasn't completed yet)
    QVERIFY(mid.glassBg != lightBg || mid.borderColor != lightBorder || mid.accentColor != lightAccent);

    // Wait for completion
    QTest::qWait(350);

    auto final = dock.currentThemeColors();
    QCOMPARE(final.glassBg, lightBg);
    QCOMPARE(final.borderColor, lightBorder);
    QCOMPARE(final.accentColor, lightAccent);
    QVERIFY(!final.isDark);
}

// ── Tests for ThemeManager integration ────────────────────────────────

void TestApplicationDock::testSetThemeManager()
{
    ThemeManager themeManager;
    // Apply a known dark theme initially
    ThemeManager::Theme darkTheme(ThemeManager::ColorFamily::Default, ThemeManager::Mode::Dark);
    themeManager.applyTheme(darkTheme);

    ApplicationDock dock;
    dock.setThemeManager(&themeManager);

    // Allow animation to complete
    QTest::qWait(350);

    auto colors = dock.currentThemeColors();
    QVERIFY(colors.isDark);
    QVERIFY(colors.glassBg.isValid());
    QVERIFY(colors.accentColor.isValid());

    // Switch theme to light via ThemeManager
    ThemeManager::Theme lightTheme(ThemeManager::ColorFamily::Default, ThemeManager::Mode::Light);
    themeManager.applyTheme(lightTheme);

    QTest::qWait(350);

    auto lightColors = dock.currentThemeColors();
    QVERIFY(!lightColors.isDark);
    QVERIFY(lightColors.glassBg.isValid());

    // Verify the accent color matches the theme's highlight color
    // (approximate — theme definition may apply subtle adjustments)
    QColor themeAccent = themeManager.accentColor();
    // The dock accent should now match (or be close to) the theme accent
    // Allow for slight differences due to animation precision
    int dr = qAbs(lightColors.accentColor.red() - themeAccent.red());
    int dg = qAbs(lightColors.accentColor.green() - themeAccent.green());
    int db = qAbs(lightColors.accentColor.blue() - themeAccent.blue());
    QVERIFY2(dr <= 2 && dg <= 2 && db <= 2,
             qPrintable(QString("Accent colors differ: dock=%1, theme=%2")
                        .arg(lightColors.accentColor.name(), themeAccent.name())));
}
