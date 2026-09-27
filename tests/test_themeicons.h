#ifndef TEST_THEMEICONS_H
#define TEST_THEMEICONS_H

#include <QObject>

class TestThemeIcons : public QObject
{
    Q_OBJECT
private slots:
    void testRoles();
    void testSingletonInstance();
    void testRecolorAll();

    // Tint output is correct (shape from the SVG's alpha, colour from the role)
    void testTintKeepsSvgShapeAndUsesRoleColour();
    void testTintIsTransparentOutsideShape();
    void testMissingResourceYieldsNullPixmap();

    // Caching: the second request for the same (path, size, colour) must not
    // rasterise again.
    void testRepeatedRequestHitsCache();
    void testDifferentSizeIsSeparateCacheEntry();
    void testRecolorInvalidatesTintCacheButKeepsParsers();
    void testFileIconProviderMemoisesAcrossQueries();
    void testEntryReplacedNotDuplicatedOnReregistration();
    void testDestroyedTargetIsPruned();

    // High-DPI: the pixmap must be rasterised at logical size * dpr and carry
    // that ratio so QIcon does not upscale a 1x image on a fractional display.
    void testDevicePixelRatioIsTagged();
    void testCacheKeyDistinguishesCollidingDeviceSizes();
};

#endif
