#ifndef BOOKMARKMANAGER_H
#define BOOKMARKMANAGER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QString>

class QPlainTextEdit;

/**
 * Represents a bookmarked line in the editor
 */
struct Bookmark {
    QString filePath;       // File containing the bookmark
    int line;               // Line number (0-based)
    QString text;           // Line text at time of bookmarking
    int id;                 // Unique bookmark ID
};

/**
 * Manages bookmarks across multiple files.
 * Supports toggling bookmarks, navigating between them, and persisting to settings.
 */
class BookmarkManager : public QObject
{
    Q_OBJECT
public:
    explicit BookmarkManager(QObject *parent = nullptr);

    // Core operations
    int toggleBookmark(const QString &filePath, int line, const QString &text = QString());
    void removeBookmark(int id);
    void removeAllBookmarks();
    void removeAllBookmarksForFile(const QString &filePath);
    
    // Navigation
    void nextBookmark();
    void previousBookmark();
    void goToBookmark(int id);
    
    // Query
    bool isBookmarked(const QString &filePath, int line) const;
    int bookmarkAt(const QString &filePath, int line) const;
    const QList<Bookmark>& bookmarks() const { return m_bookmarks; }
    QList<Bookmark> bookmarksForFile(const QString &filePath) const;
    int bookmarkCount() const { return m_bookmarks.size(); }
    
    // Persistence
    void saveToSettings();
    void loadFromSettings();
    
    // Configuration
    void setEditor(QPlainTextEdit *editor) { m_editor = editor; }
    
signals:
    void bookmarkToggled(int id, const QString &filePath, int line, bool added);
    void bookmarkNavigated(int id, const QString &filePath, int line);
    void bookmarksChanged();

private:
    int generateId() const;
    int findNextBookmarkIndex(int currentLine, const QString &filePath) const;
    int findPreviousBookmarkIndex(int currentLine, const QString &filePath) const;
    
    QPlainTextEdit *m_editor;
    QList<Bookmark> m_bookmarks;
    int m_nextId;
    int m_currentIndex;  // For navigation tracking
};

#endif // BOOKMARKMANAGER_H
