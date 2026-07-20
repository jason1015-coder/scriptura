#ifndef LANGUAGEREGISTRY_H
#define LANGUAGEREGISTRY_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QRegularExpression>
#include <QTextCharFormat>

/**
 * Data-driven language definition system for Scriptura.
 * Replaces the hardcoded setupXxx() methods in CodeHighlighter
 * with a registry of language definitions, each specifying
 * file extensions, keywords, and comment styles.
 */

struct LanguageDefinition {
    QString name;                          // Lowercase canonical name (e.g. "python")
    QStringList extensions;                // File extensions without dot (e.g. "py", "pyw")
    QStringList keywords;                  // Reserved keywords for this language
    QStringList builtins;                  // Built-in functions/types for this language
    QString blockCommentStart;             // e.g. "/*"
    QString blockCommentEnd;              // e.g. "*/"
    QString lineComment;                   // e.g. "//"
    bool hasCStyleComments = false;        // Uses /* */ and // style comments
    bool hasHtmlComments = false;          // Uses <!-- --> style comments
    bool hasPythonTripleStrings = false;   // Uses """ and ''' for multi-line strings
    bool hasBracketMatching = true;        // Whether to highlight matching brackets
    QStringList stringDelimiters = {"\"", "'"};  // String delimiters
    QStringList multiLineStringDelimiters;       // Multi-line string delimiters e.g. """ and '''
    QString templateStringDelimiter;             // e.g. "`" for JS/TS template literals
};

class LanguageRegistry
{
public:
    static LanguageRegistry& instance();

    void registerLanguage(const LanguageDefinition &def);
    const LanguageDefinition* findByName(const QString &name) const;
    const LanguageDefinition* findByExtension(const QString &extension) const;
    QStringList allLanguageNames() const;
    const QVector<LanguageDefinition>& allLanguages() const;
    QString languageForFile(const QString &filePath) const;

private:
    LanguageRegistry();
    ~LanguageRegistry() = default;
    LanguageRegistry(const LanguageRegistry&) = delete;
    LanguageRegistry& operator=(const LanguageRegistry&) = delete;

    void registerBuiltinLanguages();
    void registerPython();
    void registerCStyle();
    void registerJava();
    void registerJavaScript();
    void registerTypeScript();
    void registerRust();
    void registerGo();
    void registerShell();
    void registerHtml();
    void registerCss();
    void registerScript();

    // Additional languages
    void registerSwift();
    void registerKotlin();
    void registerRuby();
    void registerPhp();
    void registerCsharp();
    void registerDart();
    void registerLua();
    void registerR();
    void registerScala();
    void registerObjectiveC();
    void registerYaml();
    void registerToml();
    void registerJson();
    void registerMarkdown();
    void registerSql();
    void registerPerl();
    void registerHaskell();
    void registerElixir();

    QVector<LanguageDefinition> m_languages;
};

#endif // LANGUAGEREGISTRY_H
