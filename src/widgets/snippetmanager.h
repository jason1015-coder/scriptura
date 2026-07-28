#ifndef SNIPPETMANAGER_H
#define SNIPPETMANAGER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QString>

class QPlainTextEdit;

/**
 * Represents a code snippet with placeholders
 */
struct Snippet {
    QString id;           // Unique identifier
    QString name;         // Display name
    QString prefix;       // Trigger prefix
    QString body;         // Snippet body with placeholders
    QString description;  // Description
    QString language;     // Language scope
    int tabStops;         // Number of tab stops
};

/**
 * Manages code snippets with tab stops, variables, and placeholders.
 * Features:
 * - Create/edit/delete snippets
 * - Tab stop navigation (1, 2, 3, etc.)
 * - Variable substitution ($CURRENT_DATE, $FILENAME, etc.)
 * - Language-scoped snippets
 * - Import/export snippets
 */
class SnippetManager : public QObject
{
    Q_OBJECT
public:
    explicit SnippetManager(QObject *parent = nullptr);

    // Core operations
    void addSnippet(const Snippet &snippet);
    void updateSnippet(const Snippet &snippet);
    void removeSnippet(const QString &id);
    
    // Query
    Snippet snippetById(const QString &id) const;
    QList<Snippet> snippetsForLanguage(const QString &language) const;
    QList<Snippet> allSnippets() const { return m_snippets; }
    QStringList snippetPrefixes() const;
    
    // Insertion
    void insertSnippet(QPlainTextEdit *editor, const Snippet &snippet);
    bool hasSnippetForPrefix(const QString &prefix, const QString &language) const;
    Snippet snippetForPrefix(const QString &prefix, const QString &language) const;
    
    // Tab stop navigation
    bool hasTabStops() const { return !m_tabStops.isEmpty(); }
    void nextTabStop(QPlainTextEdit *editor);
    void previousTabStop(QPlainTextEdit *editor);
    void clearTabStops();
    
    // Persistence
    void saveToSettings();
    void loadFromSettings();
    
    // Import/Export
    bool importSnippets(const QString &filePath);
    bool exportSnippets(const QString &filePath) const;

signals:
    void snippetInserted(const QString &id);
    void snippetAdded(const QString &id);
    void snippetRemoved(const QString &id);
    void snippetsChanged();

private:
    void parseTabStops(const QString &body, QList<QPair<int, QString>> &stops) const;
    QString substituteVariables(const QString &text) const;
    int findTabStop(const QString &body, int startPos = 0) const;
    
    QList<Snippet> m_snippets;
    
    // Tab stop state
    struct TabStop {
        int position;
        int length;
        QString placeholder;
    };
    QList<TabStop> m_tabStops;
    int m_currentTabStopIndex;
};

#endif // SNIPPETMANAGER_H
