#include "test_fileicons.h"

#include "fileicons.h"
#include "languageregistry.h"
#include "themeicons.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPixmap>
#include <QTemporaryDir>
#include <QTest>

namespace {
// Any pixel at least this opaque counts as ink; the tint step uses the SVG's
// alpha as a mask, so "drew something" is exactly "alpha is not ~zero".
constexpr int kInkAlpha = 8;

bool hasInk(const QImage &image, const QRect &region)
{
    const QRect r = region.intersected(image.rect());
    for (int y = r.top(); y <= r.bottom(); ++y) {
        for (int x = r.left(); x <= r.right(); ++x) {
            if (qAlpha(image.pixel(x, y)) > kInkAlpha)
                return true;
        }
    }
    return false;
}
} // namespace

void TestFileIcons::testEveryRegisteredLanguageHasAnIcon()
{
    const QSet<QString> covered = FileIcons::coveredLanguages();
    const QStringList registered = LanguageRegistry::instance().allLanguageNames();

    for (const QString &name : registered) {
        QVERIFY2(covered.contains(name),
                 qPrintable(QStringLiteral("no icon declared for registered language '%1'")
                                .arg(name)));
    }
    // The reverse direction too: an icon for a language nobody registers would
    // be dead weight, and usually a typo in a language name.
    for (const QString &name : covered) {
        QVERIFY2(registered.contains(name),
                 qPrintable(QStringLiteral("icon declared for unregistered language '%1'")
                                .arg(name)));
    }
}

void TestFileIcons::testLanguageIconsAreUnique()
{
    QHash<QString, QString> iconToLanguage;
    for (const QString &name : FileIcons::coveredLanguages()) {
        const QString path = FileIcons::forLanguage(name);
        QVERIFY2(!path.isEmpty(),
                 qPrintable(QStringLiteral("language '%1' resolves to no icon").arg(name)));
        if (iconToLanguage.contains(path)) {
            QFAIL(qPrintable(QStringLiteral("languages '%1' and '%2' share icon %3")
                                 .arg(iconToLanguage.value(path), name, path)));
        }
        iconToLanguage.insert(path, name);
    }
    QCOMPARE(iconToLanguage.size(), FileIcons::coveredLanguages().size());
}

void TestFileIcons::testAllReferencedIconsExistInResources()
{
    const QSet<QString> paths = FileIcons::allIconPaths();
    QVERIFY(!paths.isEmpty());
    for (const QString &path : paths) {
        QVERIFY2(QFile::exists(path),
                 qPrintable(QStringLiteral("icon resource missing: %1").arg(path)));
        // A resource that exists but is empty would tint to a blank pixmap.
        QVERIFY2(QFileInfo(path).size() > 0,
                 qPrintable(QStringLiteral("icon resource is empty: %1").arg(path)));
    }
}

void TestFileIcons::testEveryIconRendersVisibleInk()
{
    // A resource that exists but draws nothing tints to an invisible pixmap, and
    // no lookup test would ever notice. Rasterising each icon and demanding ink
    // turns "the file is there" into "the glyph is actually visible".
    ThemeIcons *icons = ThemeIcons::instance();
    for (const QString &path : FileIcons::allIconPaths()) {
        const QImage image = icons->pixmap(path, ThemeIcons::Role::Normal, 32)
                                 .toImage().convertToFormat(QImage::Format_ARGB32);
        QVERIFY2(!image.isNull(),
                 qPrintable(QStringLiteral("no pixmap rendered for %1").arg(path)));
        QVERIFY2(hasInk(image, image.rect()),
                 qPrintable(QStringLiteral("icon rendered blank: %1").arg(path)));
    }
}

void TestFileIcons::testLetterformIconsDrawTheirLabel()
{
    // The language badges spell their label with <text>. The badge frame alone
    // would still put ink in the tile's corners, so require ink in the central
    // band too: for these icons the frame sits near the tile edge and only the
    // label can reach the middle. That is what distinguishes "the text rendered"
    // from "the badge drew but the label silently vanished".
    ThemeIcons *icons = ThemeIcons::instance();
    int letterformIcons = 0;
    for (const QString &path : FileIcons::allIconPaths()) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || !file.readAll().contains("<text"))
            continue; // pictorial icon (folder, close, git…): nothing to assert
        ++letterformIcons;

        const QImage image = icons->pixmap(path, ThemeIcons::Role::Normal, 48)
                                 .toImage().convertToFormat(QImage::Format_ARGB32);
        const int inset = image.width() / 4; // central half of the tile
        const QRect middle(inset, inset, image.width() - 2 * inset,
                           image.height() - 2 * inset);
        QVERIFY2(hasInk(image, middle),
                 qPrintable(QStringLiteral("label did not render for %1").arg(path)));
    }
    // Guard against the loop silently skipping everything and passing anyway:
    // the letterform set is a known, fixed size.
    QVERIFY2(letterformIcons >= 28,
             qPrintable(QStringLiteral("only %1 letterform icons found").arg(letterformIcons)));
}

void TestFileIcons::testResolvesEachLanguageByExtension()
{
    // One representative file per registered language, with the extension
    // LanguageRegistry itself claims for it.
    const QHash<QString, QString> samples = {
        { QStringLiteral("main.py"),     QStringLiteral("python") },
        { QStringLiteral("main.cpp"),    QStringLiteral("cpp") },
        { QStringLiteral("Main.java"),   QStringLiteral("java") },
        { QStringLiteral("app.js"),      QStringLiteral("javascript") },
        { QStringLiteral("app.ts"),      QStringLiteral("typescript") },
        { QStringLiteral("lib.rs"),      QStringLiteral("rust") },
        { QStringLiteral("main.go"),     QStringLiteral("go") },
        { QStringLiteral("run.sh"),      QStringLiteral("shell") },
        { QStringLiteral("page.html"),   QStringLiteral("html") },
        { QStringLiteral("site.css"),    QStringLiteral("css") },
        { QStringLiteral("App.swift"),   QStringLiteral("swift") },
        { QStringLiteral("Main.kt"),     QStringLiteral("kotlin") },
        { QStringLiteral("app.rb"),      QStringLiteral("ruby") },
        { QStringLiteral("index.php"),   QStringLiteral("php") },
        { QStringLiteral("Program.cs"),  QStringLiteral("csharp") },
        { QStringLiteral("main.dart"),   QStringLiteral("dart") },
        { QStringLiteral("init.lua"),    QStringLiteral("lua") },
        { QStringLiteral("plot.R"),      QStringLiteral("r") },
        { QStringLiteral("Main.scala"),  QStringLiteral("scala") },
        { QStringLiteral("main.m"),      QStringLiteral("objectivec") },
        { QStringLiteral("ci.yaml"),     QStringLiteral("yaml") },
        { QStringLiteral("Cargo.toml"),  QStringLiteral("toml") },
        { QStringLiteral("data.json"),   QStringLiteral("json") },
        { QStringLiteral("README.md"),   QStringLiteral("markdown") },
        { QStringLiteral("query.sql"),   QStringLiteral("sql") },
        { QStringLiteral("tool.pl"),     QStringLiteral("perl") },
        { QStringLiteral("Main.hs"),     QStringLiteral("haskell") },
        { QStringLiteral("mix.exs"),     QStringLiteral("elixir") },
    };

    QCOMPARE(samples.size(), 28);
    for (auto it = samples.constBegin(); it != samples.constEnd(); ++it) {
        const QString expected = FileIcons::forLanguage(it.value());
        QVERIFY2(!expected.isEmpty(),
                 qPrintable(QStringLiteral("language %1 has no icon").arg(it.value())));
        QCOMPARE(FileIcons::forFileName(it.key()), expected);
    }
}

void TestFileIcons::testExactFilenamesBeatExtensionLookup()
{
    // ".gitignore" has no suffix as far as QFileInfo is concerned, so only the
    // exact-name table can classify it.
    QCOMPARE(FileIcons::forFileName(".gitignore"), QStringLiteral(":/icons/gitignore.svg"));
    QCOMPARE(FileIcons::forFileName("LICENSE"),      QStringLiteral(":/icons/license.svg"));
    QCOMPARE(FileIcons::forFileName("Makefile"),     QStringLiteral(":/icons/build.svg"));
    // A specific name must win over the language its extension implies, which
    // is why tsconfig.json is a config glyph and not the JSON glyph.
    QVERIFY(FileIcons::forFileName("tsconfig.json")
            != FileIcons::forFileName("other.json"));
    QCOMPARE(FileIcons::forFileName("other.json"), QStringLiteral(":/icons/json.svg"));
}

void TestFileIcons::testDotfilesResolveWithoutAKnownExtension()
{
    // Qt does report a suffix for a dotfile, and it is the text after the *first*
    // dot: ".env.local" yields "local". So dotfiles resolve for two different
    // reasons, and this test pins both down.
    QCOMPARE(QFileInfo(QStringLiteral(".env.local")).suffix(), QStringLiteral("local"));
    QCOMPARE(QFileInfo(QStringLiteral(".gitignore")).suffix(), QStringLiteral("gitignore"));

    // Suffix happens to be a name we know, so the extension table resolves it.
    QCOMPARE(FileIcons::forFileName(".gitignore"),     QStringLiteral(":/icons/gitignore.svg"));
    QCOMPARE(FileIcons::forFileName(".dockerignore"),  QStringLiteral(":/icons/gitignore.svg"));
    QCOMPARE(FileIcons::forFileName(".editorconfig"),  QStringLiteral(":/icons/config.svg"));
    QCOMPARE(FileIcons::forFileName(".env"),           QStringLiteral(":/icons/config.svg"));

    // Suffix is meaningless here, so only the whole-name table can classify it.
    // If step 1 of forFileName() ever loses priority, this one regresses.
    QCOMPARE(FileIcons::forFileName(".env.local"), QStringLiteral(":/icons/config.svg"));
    QCOMPARE(FileIcons::forFileName(".prettierrc"), QStringLiteral(":/icons/config.svg"));

    // A name that is neither special nor suffixed is just text.
    QCOMPARE(FileIcons::forFileName("INSTALL"), FileIcons::textIcon());
}

void TestFileIcons::testNonSourceExtensionsResolve()
{
    QCOMPARE(FileIcons::forFileName("logo.png"),   QStringLiteral(":/icons/image.svg"));
    QCOMPARE(FileIcons::forFileName("photo.JPEG"), QStringLiteral(":/icons/image.svg"));
    QCOMPARE(FileIcons::forFileName("icon.svg"),   QStringLiteral(":/icons/svg-image.svg"));
    QCOMPARE(FileIcons::forFileName("build.bat"),  QStringLiteral(":/icons/console.svg"));
    QCOMPARE(FileIcons::forFileName("setup.ps1"),  QStringLiteral(":/icons/console.svg"));
    QCOMPARE(FileIcons::forFileName("bundle.zip"), QStringLiteral(":/icons/archive.svg"));
    QCOMPARE(FileIcons::forFileName("key.pem"),    QStringLiteral(":/icons/lock.svg"));
    QCOMPARE(FileIcons::forFileName("page.xml"),   QStringLiteral(":/icons/xml.svg"));
    QCOMPARE(FileIcons::forFileName("notes.txt"),  QStringLiteral(":/icons/text.svg"));
}

void TestFileIcons::testUnknownExtensionFallsBackToGenericPage()
{
    // A recognised suffix that is not source code still gets the plain page,
    // rather than a wrong guess at its type.
    QCOMPARE(FileIcons::forFileName("model.qqq"), FileIcons::fileIcon());
    QCOMPARE(FileIcons::forFileName("data.zzz"),  FileIcons::fileIcon());
    // A name with no suffix at all is text. CHANGELOG is not a free case here:
    // the whole-name table claims it for markdown.
    QCOMPARE(FileIcons::forFileName("CHANGELOG"), FileIcons::forLanguage(QStringLiteral("markdown")));
    QCOMPARE(FileIcons::forFileName("README"),    FileIcons::forLanguage(QStringLiteral("markdown")));
    QCOMPARE(FileIcons::forFileName("INSTALL"),   FileIcons::textIcon());
}

void TestFileIcons::testExtensionlessFilesResolveToText()
{
    QCOMPARE(FileIcons::forFileName("INSTALL"), FileIcons::textIcon());
    // The registry lists "Gemfile" among Ruby's extensions, so an extensionless
    // build file must still pick up the Ruby glyph.
    QCOMPARE(FileIcons::forFileName("Gemfile"), FileIcons::forLanguage(QStringLiteral("ruby")));
}

void TestFileIcons::testDirectoryNamesResolve()
{
    QCOMPARE(FileIcons::forDirectoryName("src"),     QStringLiteral(":/icons/folder-code.svg"));
    QCOMPARE(FileIcons::forDirectoryName("tests"),   QStringLiteral(":/icons/folder-test.svg"));
    QCOMPARE(FileIcons::forDirectoryName("assets"),  QStringLiteral(":/icons/folder-image.svg"));
    QCOMPARE(FileIcons::forDirectoryName(".git"),    QStringLiteral(":/icons/folder-git.svg"));
    QCOMPARE(FileIcons::forDirectoryName(".github"), QStringLiteral(":/icons/github.svg"));
    // Anything unrecognised stays a plain folder rather than losing its glyph.
    QCOMPARE(FileIcons::forDirectoryName("zzz"),  FileIcons::folderIcon());
    QCOMPARE(FileIcons::forDirectoryName(""),     FileIcons::folderIcon());
}

void TestFileIcons::testForPathDispatchesOnDirectoryOrFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString subDir = dir.path() + QStringLiteral("/src");
    QVERIFY(QDir().mkpath(subDir));
    const QString file = dir.path() + QStringLiteral("/main.py");
    QFile f(file);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();

    QCOMPARE(FileIcons::forPath(subDir), QStringLiteral(":/icons/folder-code.svg"));
    QCOMPARE(FileIcons::forPath(file), FileIcons::forLanguage(QStringLiteral("python")));
}

void TestFileIcons::testUnsavedBufferResolvesToText()
{
    // An editor with no path has no type to report.
    QCOMPARE(FileIcons::forFileName(QString()), FileIcons::textIcon());
    QCOMPARE(FileIcons::forPath(QString()), FileIcons::textIcon());
}

void TestFileIcons::testResolutionIsCaseInsensitive()
{
    // The file tree happily shows files the registry would not have matched
    // verbatim, e.g. README.MD on a case-sensitive filesystem.
    QCOMPARE(FileIcons::forFileName("README.MD"), FileIcons::forFileName("readme.md"));
    QCOMPARE(FileIcons::forFileName("Main.PY"),   FileIcons::forFileName("main.py"));
    QCOMPARE(FileIcons::forDirectoryName("SRC"),   FileIcons::forDirectoryName("src"));
}
