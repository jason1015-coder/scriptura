#include "fileicons.h"
#include "languageregistry.h"

#include <QFileInfo>

namespace {
QString icon(const char *name)
{
    return QStringLiteral(":/icons/") + QLatin1String(name) + QStringLiteral(".svg");
}
} // namespace

const QHash<QString, QString> &FileIcons::fileNameTable()
{
    // Keys are lowercased base names. Anything with a real extension belongs in
    // extensionTable() instead; this table is for names whose *whole spelling*
    // is the signal.
    static const QHash<QString, QString> table = {
        // ── Version control ───────────────────────────────────────────
        { QStringLiteral(".gitignore"),        icon("gitignore") },
        { QStringLiteral(".gitattributes"),    icon("git-file") },
        { QStringLiteral(".gitmodules"),       icon("git-file") },
        { QStringLiteral(".gitconfig"),        icon("git-file") },
        { QStringLiteral(".git-blame-ignore-revs"), icon("gitignore") },

        // ── Project / legal ───────────────────────────────────────────
        { QStringLiteral("license"),           icon("license") },
        { QStringLiteral("licence"),           icon("license") },
        { QStringLiteral("license.md"),        icon("license") },
        { QStringLiteral("license.txt"),       icon("license") },
        { QStringLiteral("copying"),           icon("license") },
        { QStringLiteral("notice"),            icon("license") },
        { QStringLiteral("unlicense"),         icon("license") },
        { QStringLiteral("authors"),           icon("license") },
        { QStringLiteral("contributors"),      icon("license") },
        { QStringLiteral("readme"),            icon("markdown") },
        { QStringLiteral("changelog"),         icon("markdown") },
        { QStringLiteral("changes"),           icon("markdown") },
        { QStringLiteral("contributing"),      icon("markdown") },
        { QStringLiteral("code_of_conduct"),   icon("markdown") },

        // ── Toolchain ─────────────────────────────────────────────────
        { QStringLiteral("makefile"),          icon("build") },
        { QStringLiteral("gnumakefile"),       icon("build") },
        { QStringLiteral("cmakelists.txt"),    icon("build") },
        { QStringLiteral("dockerfile"),        icon("build") },
        { QStringLiteral("containerfile"),     icon("build") },
        { QStringLiteral("justfile"),          icon("build") },
        { QStringLiteral("rakefile"),          icon("build") },
        { QStringLiteral("procfile"),          icon("build") },
        { QStringLiteral("vagrantfile"),       icon("build") },

        // ── Editor / tool configuration ───────────────────────────────
        { QStringLiteral(".editorconfig"),     icon("config") },
        { QStringLiteral(".env"),              icon("config") },
        { QStringLiteral(".env.local"),        icon("config") },
        { QStringLiteral(".env.example"),      icon("config") },
        { QStringLiteral(".env.development"),  icon("config") },
        { QStringLiteral(".env.production"),   icon("config") },
        { QStringLiteral(".env.test"),         icon("config") },
        { QStringLiteral(".npmrc"),            icon("config") },
        { QStringLiteral(".nvmrc"),            icon("config") },
        { QStringLiteral(".prettierrc"),       icon("config") },
        { QStringLiteral(".prettierrc.json"),  icon("config") },
        { QStringLiteral(".eslintrc"),         icon("config") },
        { QStringLiteral(".eslintrc.json"),    icon("config") },
        { QStringLiteral(".eslintrc.js"),      icon("config") },
        { QStringLiteral(".eslintrc.cjs"),     icon("config") },
        { QStringLiteral(".babelrc"),          icon("config") },
        { QStringLiteral(".npmignore"),        icon("gitignore") },
        { QStringLiteral(".dockerignore"),     icon("gitignore") },
        { QStringLiteral(".htaccess"),         icon("config") },
        { QStringLiteral("settings.json"),     icon("config") },
        { QStringLiteral("tsconfig.json"),     icon("config") },
        { QStringLiteral("jsconfig.json"),     icon("config") },

        // ── Credentials ───────────────────────────────────────────────
        { QStringLiteral("id_rsa"),            icon("lock") },
        { QStringLiteral("id_dsa"),            icon("lock") },
        { QStringLiteral("id_ecdsa"),          icon("lock") },
        { QStringLiteral("id_ed25519"),        icon("lock") },
        { QStringLiteral("credentials"),       icon("lock") },
        { QStringLiteral("known_hosts"),       icon("lock") },
        { QStringLiteral("authorized_keys"),   icon("lock") },
        { QStringLiteral(".netrc"),            icon("lock") },
        { QStringLiteral(".pgpass"),           icon("lock") },
        { QStringLiteral("secrets.yaml"),      icon("lock") },
        { QStringLiteral("secrets.json"),      icon("lock") },
    };
    return table;
}

const QHash<QString, QString> &FileIcons::extensionTable()
{
    // Consulted before LanguageRegistry, so only extensions that no registered
    // language claims live here. A language extension must never be duplicated
    // below, or the two tables would disagree about the same file.
    static const QHash<QString, QString> table = {
        // ── Images ───────────────────────────────────────────────────
        { QStringLiteral("png"),      icon("image") },
        { QStringLiteral("jpg"),      icon("image") },
        { QStringLiteral("jpeg"),     icon("image") },
        { QStringLiteral("jpe"),      icon("image") },
        { QStringLiteral("gif"),      icon("image") },
        { QStringLiteral("bmp"),      icon("image") },
        { QStringLiteral("webp"),     icon("image") },
        { QStringLiteral("tif"),      icon("image") },
        { QStringLiteral("tiff"),     icon("image") },
        { QStringLiteral("ico"),      icon("image") },
        { QStringLiteral("avif"),     icon("image") },
        { QStringLiteral("heic"),     icon("image") },
        { QStringLiteral("psd"),      icon("image") },
        { QStringLiteral("xcf"),      icon("image") },
        { QStringLiteral("svg"),      icon("svg-image") },
        { QStringLiteral("svgz"),     icon("svg-image") },

        // ── Shell / Windows scripts ───────────────────────────────────
        { QStringLiteral("bat"),      icon("console") },
        { QStringLiteral("cmd"),      icon("console") },
        { QStringLiteral("ps1"),      icon("console") },
        { QStringLiteral("psm1"),     icon("console") },
        { QStringLiteral("psd1"),     icon("console") },
        { QStringLiteral("vbs"),      icon("console") },
        { QStringLiteral("desktop"),  icon("console") },
        { QStringLiteral("reg"),      icon("config") },

        // ── Archives ──────────────────────────────────────────────────
        { QStringLiteral("zip"),      icon("archive") },
        { QStringLiteral("tar"),      icon("archive") },
        { QStringLiteral("gz"),       icon("archive") },
        { QStringLiteral("tgz"),      icon("archive") },
        { QStringLiteral("bz2"),      icon("archive") },
        { QStringLiteral("xz"),       icon("archive") },
        { QStringLiteral("7z"),       icon("archive") },
        { QStringLiteral("rar"),      icon("archive") },
        { QStringLiteral("zst"),      icon("archive") },
        // NB: .jar is deliberately absent. LanguageRegistry claims it as a
        // Java extension, and the language lookup runs after this table, so
        // listing it here would silently steal it from the Java icon.
        { QStringLiteral("war"),      icon("archive") },
        { QStringLiteral("whl"),      icon("archive") },
        { QStringLiteral("deb"),      icon("archive") },
        { QStringLiteral("rpm"),      icon("archive") },

        // ── Keys and certificates ─────────────────────────────────────
        { QStringLiteral("pem"),      icon("lock") },
        { QStringLiteral("key"),      icon("lock") },
        { QStringLiteral("p12"),      icon("lock") },
        { QStringLiteral("pfx"),      icon("lock") },
        { QStringLiteral("crt"),      icon("lock") },
        { QStringLiteral("cer"),      icon("lock") },
        { QStringLiteral("asc"),      icon("lock") },
        { QStringLiteral("gpg"),      icon("lock") },
        { QStringLiteral("ppk"),      icon("lock") },

        // ── Data and documents that are not source code ──────────────
        { QStringLiteral("xml"),      icon("xml") },
        { QStringLiteral("xsd"),      icon("xml") },
        { QStringLiteral("xsl"),      icon("xml") },
        { QStringLiteral("plist"),    icon("xml") },
        { QStringLiteral("proto"),    icon("xml") },
        { QStringLiteral("txt"),      icon("text") },
        { QStringLiteral("text"),     icon("text") },
        { QStringLiteral("log"),      icon("text") },
        { QStringLiteral("in"),       icon("text") },
        { QStringLiteral("out"),      icon("text") },
        { QStringLiteral("me"),       icon("text") },
        { QStringLiteral("1st"),      icon("text") },

        // ── Structured data ───────────────────────────────────────────
        { QStringLiteral("csv"),      icon("config") },
        { QStringLiteral("tsv"),      icon("config") },
        { QStringLiteral("ini"),      icon("config") },
        { QStringLiteral("cfg"),      icon("config") },
        { QStringLiteral("conf"),     icon("config") },
        { QStringLiteral("properties"), icon("config") },
        { QStringLiteral("env"),      icon("config") },
        { QStringLiteral("editorconfig"), icon("config") },
        { QStringLiteral("lock"),     icon("config") },
        { QStringLiteral("lockb"),    icon("config") },
    };
    return table;
}

const QHash<QString, QString> &FileIcons::languageTable()
{
    // One entry per canonical name in LanguageRegistry. TestFileIcons asserts
    // the two sets are equal, so registering a new language without an icon
    // here is a build-breaking omission rather than a silent fallback.
    static const QHash<QString, QString> table = {
        { QStringLiteral("python"),     icon("python") },
        { QStringLiteral("cpp"),        icon("cpp") },
        { QStringLiteral("java"),       icon("java") },
        { QStringLiteral("javascript"), icon("javascript") },
        { QStringLiteral("typescript"), icon("typescript") },
        { QStringLiteral("rust"),       icon("rust") },
        { QStringLiteral("go"),         icon("go") },
        { QStringLiteral("shell"),      icon("shell") },
        { QStringLiteral("html"),       icon("html") },
        { QStringLiteral("css"),        icon("css") },
        { QStringLiteral("swift"),      icon("swift") },
        { QStringLiteral("kotlin"),     icon("kotlin") },
        { QStringLiteral("ruby"),       icon("ruby") },
        { QStringLiteral("php"),        icon("php") },
        { QStringLiteral("csharp"),     icon("csharp") },
        { QStringLiteral("dart"),       icon("dart") },
        { QStringLiteral("lua"),        icon("lua") },
        { QStringLiteral("r"),          icon("r") },
        { QStringLiteral("scala"),      icon("scala") },
        { QStringLiteral("objectivec"), icon("objectivec") },
        { QStringLiteral("yaml"),       icon("yaml") },
        { QStringLiteral("toml"),       icon("toml") },
        { QStringLiteral("json"),       icon("json") },
        { QStringLiteral("markdown"),   icon("markdown") },
        { QStringLiteral("sql"),        icon("sql") },
        { QStringLiteral("perl"),       icon("perl") },
        { QStringLiteral("haskell"),    icon("haskell") },
        { QStringLiteral("elixir"),     icon("elixir") },
    };
    return table;
}

const QHash<QString, QString> &FileIcons::directoryTable()
{
    static const QHash<QString, QString> table = {
        // ── Version control ───────────────────────────────────────────
        { QStringLiteral(".git"),         icon("folder-git") },
        { QStringLiteral(".github"),      icon("github") },
        { QStringLiteral(".gitlab"),      icon("folder-git") },
        { QStringLiteral(".husky"),       icon("folder-git") },
        { QStringLiteral(".circleci"),    icon("folder-git") },

        // ── Source trees ──────────────────────────────────────────────
        { QStringLiteral("src"),          icon("folder-code") },
        { QStringLiteral("source"),       icon("folder-code") },
        { QStringLiteral("sources"),      icon("folder-code") },
        { QStringLiteral("include"),      icon("folder-code") },
        { QStringLiteral("lib"),          icon("folder-code") },
        { QStringLiteral("libs"),         icon("folder-code") },
        { QStringLiteral("app"),          icon("folder-code") },
        { QStringLiteral("pkg"),          icon("folder-code") },
        { QStringLiteral("internal"),     icon("folder-code") },
        { QStringLiteral("modules"),      icon("folder-code") },

        // ── Tests ─────────────────────────────────────────────────────
        { QStringLiteral("test"),         icon("folder-test") },
        { QStringLiteral("tests"),        icon("folder-test") },
        { QStringLiteral("spec"),         icon("folder-test") },
        { QStringLiteral("specs"),        icon("folder-test") },
        { QStringLiteral("__tests__"),    icon("folder-test") },
        { QStringLiteral("testing"),      icon("folder-test") },
        { QStringLiteral("e2e"),          icon("folder-test") },
        { QStringLiteral("fixtures"),     icon("folder-test") },

        // ── Static assets ─────────────────────────────────────────────
        { QStringLiteral("assets"),       icon("folder-image") },
        { QStringLiteral("images"),       icon("folder-image") },
        { QStringLiteral("img"),          icon("folder-image") },
        { QStringLiteral("icons"),        icon("folder-image") },
        { QStringLiteral("media"),        icon("folder-image") },
        { QStringLiteral("static"),       icon("folder-image") },
        { QStringLiteral("public"),       icon("folder-image") },
        { QStringLiteral("resources"),    icon("folder-image") },
        { QStringLiteral("screenshots"),  icon("folder-image") },
    };
    return table;
}

QString FileIcons::forLanguage(const QString &languageName)
{
    return languageTable().value(languageName.toLower());
}

QSet<QString> FileIcons::coveredLanguages()
{
    return QSet<QString>(languageTable().keyBegin(), languageTable().keyEnd());
}

QSet<QString> FileIcons::allIconPaths()
{
    QSet<QString> all;
    auto collect = [&all](const QHash<QString, QString> &t) {
        for (auto it = t.constBegin(); it != t.constEnd(); ++it)
            all.insert(it.value());
    };
    collect(fileNameTable());
    collect(extensionTable());
    collect(languageTable());
    collect(directoryTable());
    all.insert(textIcon());
    all.insert(fileIcon());
    all.insert(folderIcon());
    return all;
}

QString FileIcons::forDirectoryName(const QString &dirName)
{
    if (dirName.isEmpty())
        return folderIcon();
    return directoryTable().value(dirName.toLower(), folderIcon());
}

QString FileIcons::forFileName(const QString &fileName)
{
    // 0. An unsaved buffer has no name yet, so there is no type to report.
    if (fileName.isEmpty())
        return textIcon();

    const QFileInfo info(fileName);
    const QString rawName = info.fileName();
    const QString baseName = rawName.toLower();
    const QString suffix = info.suffix().toLower();

    // 1. Whole-name match, for the names that carry their meaning on their own.
    //    This step is load-bearing for dotfiles with more than one dot: Qt does
    //    report a suffix for `.env.local` (it returns "local"), which matches
    //    nothing, so only the full name resolves it.
    const QString byName = fileNameTable().value(baseName);
    if (!byName.isEmpty())
        return byName;

    // 2. Well-known non-source extension.
    const QString byExtension = extensionTable().value(suffix);
    if (!byExtension.isEmpty())
        return byExtension;

    // 3. A registered language, by extension.
    const LanguageRegistry &registry = LanguageRegistry::instance();
    if (const LanguageDefinition *def = registry.findByExtension(suffix)) {
        const QString iconPath = forLanguage(def->name);
        if (!iconPath.isEmpty())
            return iconPath;
    }

    // 4. A registered language keyed on the whole name, which is how the
    //    registry models extensionless build files. Its findByExtension()
    //    lowercases both sides, so a mixed-case entry such as "Gemfile" only
    //    resolves here, against the original spelling.
    if (const LanguageDefinition *def = registry.findByExtension(baseName)) {
        const QString iconPath = forLanguage(def->name);
        if (!iconPath.isEmpty())
            return iconPath;
    }
    for (const LanguageDefinition &def : registry.allLanguages()) {
        if (!def.extensions.contains(rawName))
            continue;
        const QString iconPath = forLanguage(def.name);
        if (!iconPath.isEmpty())
            return iconPath;
        break;
    }

    // 5. No extension at all is the signature of a plain text file, and an
    //    extension we do not recognise is a binary of unknown provenance.
    return suffix.isEmpty() ? textIcon() : fileIcon();
}

QString FileIcons::forPath(const QString &path)
{
    if (path.isEmpty())
        return textIcon();
    const QFileInfo info(path);
    return info.isDir() ? forDirectoryName(info.fileName())
                        : forFileName(info.fileName());
}
