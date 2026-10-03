#ifndef FILEICONS_H
#define FILEICONS_H

#include <QString>
#include <QHash>
#include <QSet>

/**
 * @class FileIcons
 * @brief Maps a file or directory to the resource path of its type icon.
 *
 * The file tree and the tab bar both need to answer "which glyph represents
 * this path?". FileIcons centralises the decision so the tree, the tabs and any
 * future view agree.
 *
 * Resolution order for a file (first hit wins):
 *   1. an exact filename match, so `tsconfig.json` beats the `json` extension
 *      and multi-part dotfiles like `.env.local` still resolve
 *   2. a well-known non-source extension, such as `png` or `zip`
 *   3. a LanguageRegistry language, keyed on the extension
 *   4. a LanguageRegistry language keyed on the whole name, which is how
 *      extensionless build files like `Gemfile` are covered
 *   5. text, when there is no extension at all
 *   6. a generic page
 *
 * Language icons are declared in a table keyed by the registry's canonical
 * name, so a new language in LanguageRegistry is a one-line addition here and
 * TestFileIcons fails loudly until it is made.
 */
class FileIcons
{
public:
    /**
     * @brief Icon resource path for a regular file.
     * @param fileName Base name with extension, e.g. "main.cpp". An empty
     *        string (an unsaved buffer) yields the plain-text icon.
     */
    static QString forFileName(const QString &fileName);

    /**
     * @brief Icon resource path for a directory, given its base name.
     * An empty name yields the generic folder.
     */
    static QString forDirectoryName(const QString &dirName);

    /**
     * @brief Icon resource path for a whole path, dispatching on whether the
     * final component is a directory. This is what the file tree uses.
     */
    static QString forPath(const QString &path);

    /**
     * @brief Icon resource path for a language's canonical registry name,
     * e.g. "python". Returns an empty string for unregistered languages.
     */
    static QString forLanguage(const QString &languageName);

    /**
     * @brief Every language name that currently has a dedicated icon.
     * Exposed so the test can prove full coverage of LanguageRegistry.
     */
    static QSet<QString> coveredLanguages();

    /**
     * @brief Every resource path this class can return, for test assertions
     * that each one actually exists in the compiled resource bundle.
     */
    static QSet<QString> allIconPaths();

    static QString textIcon()    { return QStringLiteral(":/icons/text.svg"); }
    static QString fileIcon()    { return QStringLiteral(":/icons/file.svg"); }
    static QString folderIcon()  { return QStringLiteral(":/icons/folder.svg"); }

private:
    /**
     * @brief Lookup table for exact filenames (lowercased).
     *
     * Covers the names that carry meaning on their own, so they are classified
     * by the whole name rather than by their extension. Keys are full base
     * names, so `.env.local` is stored in full while `.gitignore` is a bare
     * dotfile. Consulted first, which is what keeps `tsconfig.json` a config
     * glyph rather than the plain `json` one.
     */
    static const QHash<QString, QString> &fileNameTable();

    /**
     * @brief Lookup table for well-known non-source extensions (lowercased).
     * Consulted before LanguageRegistry so a `.png` never falls through to a
     * language and a `.sh` is never claimed by an image rule.
     */
    static const QHash<QString, QString> &extensionTable();

    /**
     * @brief Canonical language name -> icon path.
     */
    static const QHash<QString, QString> &languageTable();

    /**
     * @brief Directory base name -> folder-variant icon path.
     */
    static const QHash<QString, QString> &directoryTable();
};

#endif // FILEICONS_H
