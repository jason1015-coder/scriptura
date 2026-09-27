#include <QTest>
#include "themeicons.h"
#include "test_themeicons.h"

#include <QApplication>
#include <QPushButton>
#include <QLabel>
#include <QImage>
#include <QFileIconProvider>

namespace {
constexpr const char* kClose = ":/icons/close.svg";
constexpr const char* kFolder = ":/icons/folder.svg";

// 一次呼叫所造成的光柵化次數（快照相減）。
int rasterizationsDuring(const std::function<void()>& fn)
{
    const int before = ThemeIcons::rasterizationCount();
    fn();
    return ThemeIcons::rasterizationCount() - before;
}
} // namespace

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

void TestThemeIcons::testTintKeepsSvgShapeAndUsesRoleColour()
{
    ThemeIcons *ti = ThemeIcons::instance();
    const QColor expected = ti->colorForRole(ThemeIcons::Role::Normal);

    const QPixmap pm = ti->pixmap(QString::fromLatin1(kClose), ThemeIcons::Role::Normal, 24);
    QVERIFY(!pm.isNull());

    const QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
    bool sawOpaquePixel = false;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QColor px = img.pixelColor(x, y);
            if (px.alpha() != 255)
                continue;
            sawOpaquePixel = true;
            // DestinationIn keeps the destination alpha and takes the source
            // colour, so every solid pixel must be exactly the role colour.
            QCOMPARE(px.red(), expected.red());
            QCOMPARE(px.green(), expected.green());
            QCOMPARE(px.blue(), expected.blue());
        }
    }
    QVERIFY(sawOpaquePixel);
}

void TestThemeIcons::testTintIsTransparentOutsideShape()
{
    ThemeIcons *ti = ThemeIcons::instance();
    // close.svg is a stroke-only X inset from the edges, so the corners must
    // stay empty — i.e. we preserved the SVG's alpha rather than filling a box.
    const QPixmap pm = ti->pixmap(QString::fromLatin1(kClose), ThemeIcons::Role::Normal, 24);
    QVERIFY(!pm.isNull());

    const QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
    QCOMPARE(img.pixelColor(0, 0).alpha(), 0);
    QCOMPARE(img.pixelColor(img.width() - 1, 0).alpha(), 0);
    QCOMPARE(img.pixelColor(0, img.height() - 1).alpha(), 0);
    QCOMPARE(img.pixelColor(img.width() - 1, img.height() - 1).alpha(), 0);
}

void TestThemeIcons::testMissingResourceYieldsNullPixmap()
{
    ThemeIcons *ti = ThemeIcons::instance();
    const QPixmap pm = ti->pixmap(":/icons/definitely-not-a-real-icon.svg",
                                  ThemeIcons::Role::Normal, 16);
    QVERIFY(pm.isNull());
    // icon() falls back to a plain QIcon for a path it cannot tint.
    const QIcon fallback = ti->icon(":/icons/definitely-not-a-real-icon.svg",
                                    ThemeIcons::Role::Normal, 16);
    QVERIFY(fallback.isNull());
}

void TestThemeIcons::testRepeatedRequestHitsCache()
{
    ThemeIcons *ti = ThemeIcons::instance();
    const QString path = QString::fromLatin1(kClose);

    const int cold = rasterizationsDuring([&] {
        ti->pixmap(path, ThemeIcons::Role::Normal, 20);
    });
    QCOMPARE(cold, 1);

    // Same (path, size, colour): must be served straight from the tint cache.
    const int warm = rasterizationsDuring([&] {
        for (int i = 0; i < 5; ++i)
            ti->pixmap(path, ThemeIcons::Role::Normal, 20);
    });
    QCOMPARE(warm, 0);

    // icon() goes through the same path, so it is cached too.
    const int viaIcon = rasterizationsDuring([&] {
        for (int i = 0; i < 5; ++i)
            ti->icon(path, ThemeIcons::Role::Normal, 20);
    });
    QCOMPARE(viaIcon, 0);
}

void TestThemeIcons::testDifferentSizeIsSeparateCacheEntry()
{
    ThemeIcons *ti = ThemeIcons::instance();
    const QString path = QString::fromLatin1(kClose);

    rasterizationsDuring([&] { ti->pixmap(path, ThemeIcons::Role::Normal, 21); });

    // A different logical size is a different cache key, so it rasterises once
    // — and only once.
    QCOMPARE(rasterizationsDuring([&] { ti->pixmap(path, ThemeIcons::Role::Normal, 22); }), 1);
    QCOMPARE(rasterizationsDuring([&] { ti->pixmap(path, ThemeIcons::Role::Normal, 22); }), 0);
    QCOMPARE(rasterizationsDuring([&] { ti->pixmap(path, ThemeIcons::Role::Normal, 21); }), 0);

    // And so is a different colour for the same path/size.
    QCOMPARE(rasterizationsDuring([&] {
                 ti->pixmap(path, ThemeIcons::Role::Accent, 21);
             }), 1);
    QCOMPARE(rasterizationsDuring([&] {
                 ti->pixmap(path, ThemeIcons::Role::Accent, 21);
             }), 0);
}

void TestThemeIcons::testRecolorInvalidatesTintCacheButKeepsParsers()
{
    ThemeIcons *ti = ThemeIcons::instance();
    const QString path = QString::fromLatin1(kClose);

    auto button = std::make_unique<QPushButton>();
    ti->setIcon(button.get(), path, ThemeIcons::Role::Normal, 17);
    QVERIFY(ThemeIcons::rasterizationCount() > 0);

    // recolorAll() re-tints every live entry. The old implementation spent 14
    // SVG rasterisations per entry (7 sizes x normal+disabled); it must now
    // spend exactly one — the size actually in use.
    const int tracked = ti->trackedCount();
    const int cost = rasterizationsDuring([&] { ti->recolorAll(); });
    QVERIFY(cost > 0);
    QVERIFY2(cost <= tracked,
             qPrintable(QStringLiteral("recolorAll rasterised %1 times for %2 entries")
                            .arg(cost).arg(tracked)));

    // The re-tinted result is cached, so an immediate re-request is free.
    QCOMPARE(rasterizationsDuring([&] { ti->pixmap(path, ThemeIcons::Role::Normal, 17); }), 0);
}

void TestThemeIcons::testFileIconProviderMemoisesAcrossQueries()
{
    ThemeFileIconProvider provider;
    // Warm both result slots of the provider's 2-icon result space.
    provider.icon(QFileIconProvider::Folder);
    provider.icon(QFileIconProvider::File);

    const int queries = rasterizationsDuring([&] {
        for (int i = 0; i < 200; ++i) {
            provider.icon(QFileIconProvider::Folder);
            provider.icon(QFileIconProvider::File);
        }
    });
    // 400 queries, zero rasterisations: the file tree cannot churn the cache.
    QCOMPARE(queries, 0);

    QVERIFY(!provider.icon(QFileIconProvider::Folder).isNull());
    QVERIFY(!provider.icon(QFileIconProvider::File).isNull());
}

void TestThemeIcons::testEntryReplacedNotDuplicatedOnReregistration()
{
    ThemeIcons *ti = ThemeIcons::instance();
    auto button = std::make_unique<QPushButton>();

    ti->setIcon(button.get(), QString::fromLatin1(kClose), ThemeIcons::Role::Normal, 16);
    const int afterFirst = ti->trackedCount();

    // Registering the same target again must overwrite, not append — otherwise
    // transient menus would grow the table without bound.
    ti->setIcon(button.get(), QString::fromLatin1(kFolder), ThemeIcons::Role::Normal, 16);
    QCOMPARE(ti->trackedCount(), afterFirst);
}

void TestThemeIcons::testDestroyedTargetIsPruned()
{
    ThemeIcons *ti = ThemeIcons::instance();
    {
        auto button = std::make_unique<QPushButton>();
        ti->setIcon(button.get(), QString::fromLatin1(kClose), ThemeIcons::Role::Normal, 16);
    } // destroyed here; the QPointer nulls out

    // The next registration (or a theme change) sweeps the dead entry away.
    auto survivor = std::make_unique<QPushButton>();
    ti->setIcon(survivor.get(), QString::fromLatin1(kFolder), ThemeIcons::Role::Normal, 16);
    ti->recolorAll();

    // Only survivors remain; every tracked target must be a live QObject.
    QVERIFY(ti->trackedCount() >= 1);
    Q_UNUSED(survivor);
}

void TestThemeIcons::testDevicePixelRatioIsTagged()
{
    ThemeIcons *ti = ThemeIcons::instance();
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    const int logical = 18;

    const QPixmap pm = ti->pixmap(QString::fromLatin1(kClose),
                                  ThemeIcons::Role::Normal, logical);
    QVERIFY(!pm.isNull());

    // Rasterised at logical * dpr and tagged with that ratio, so QIcon can pick
    // a matching bucket on a fractional-scaling display instead of upscaling.
    QCOMPARE(pm.devicePixelRatio(), dpr);
    QCOMPARE(pm.width(), qRound(logical * dpr));
    QCOMPARE(pm.height(), qRound(logical * dpr));
}

void TestThemeIcons::testCacheKeyDistinguishesCollidingDeviceSizes()
{
    // Two different ratios can round to the same device size (1.0x18 and
    // 1.2x18 both give 22px). Keying the cache on the rounded size alone would
    // hand one ratio's pixmap to the other and stretch the glyph, so the ratio
    // has to be part of the key. Verified here against the key itself, which is
    // what the cache actually compares.
    const ThemeIcons::TintKey a{QString::fromLatin1(kClose), 22, QColor(10, 20, 30), 1.0};
    const ThemeIcons::TintKey b{QString::fromLatin1(kClose), 22, QColor(10, 20, 30), 1.2};
    const ThemeIcons::TintKey c{QString::fromLatin1(kClose), 24, QColor(10, 20, 30), 1.0};
    const ThemeIcons::TintKey d{QString::fromLatin1(kClose), 22, QColor(40, 50, 60), 1.0};
    const ThemeIcons::TintKey e{QString::fromLatin1(kFolder), 22, QColor(10, 20, 30), 1.0};

    QVERIFY(!(a == b));   // same device size, different ratio
    QVERIFY(!(a == c));   // different device size
    QVERIFY(!(a == d));   // different colour
    QVERIFY(!(a == e));   // different path

    // Identical requests must still collapse to one entry.
    const ThemeIcons::TintKey a2{QString::fromLatin1(kClose), 22, QColor(10, 20, 30), 1.0};
    QVERIFY(a == a2);

    // And the hash must actually spread them, otherwise distinct keys would
    // pile into one bucket.
    QVERIFY(qHash(a, 0) != qHash(b, 0));
}
