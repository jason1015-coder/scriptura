#ifndef TEST_FILEICONS_H
#define TEST_FILEICONS_H

#include <QObject>

class TestFileIcons : public QObject
{
    Q_OBJECT
private slots:
    // Every language LanguageRegistry knows about must have its own icon, and
    // no two languages may share one. These two tests are the guarantee behind
    // the "each supported language has a unique icon" requirement.
    void testEveryRegisteredLanguageHasAnIcon();
    void testLanguageIconsAreUnique();

    // The icons the tables name must actually be compiled into the binary, so
    // a typo in a table entry cannot ship as a silently blank glyph.
    void testAllReferencedIconsExistInResources();

    // The language badges spell their label with <text> rather than hand-drawn
    // paths, so something has to prove the SVG text pipeline still rasterises to
    // pixels. This is the only pair of tests that fails if <text> ever stops
    // rendering (a renderer without text support, a font-less environment):
    // every other test here would keep passing over 28 blank badges.
    void testEveryIconRendersVisibleInk();
    void testLetterformIconsDrawTheirLabel();

    void testResolvesEachLanguageByExtension();
    void testExactFilenamesBeatExtensionLookup();
    void testDotfilesResolveWithoutAKnownExtension();
    void testNonSourceExtensionsResolve();
    void testUnknownExtensionFallsBackToGenericPage();
    void testExtensionlessFilesResolveToText();
    void testDirectoryNamesResolve();
    void testForPathDispatchesOnDirectoryOrFile();
    void testUnsavedBufferResolvesToText();
    void testResolutionIsCaseInsensitive();
};

#endif
