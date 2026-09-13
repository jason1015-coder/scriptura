#ifndef BOOKMARKMANAGER_H
#define BOOKMARKMANAGER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QString>

class QPlainTextEdit;
class RustBookmarkStoreAdapter;

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
 * Qt is the UI face only: all storage, toggling, navigation and persistence
 * logic lives in the Rust bookmark engine (`rust_bookmarks_*`). This class
 * forwards requests and emits Qt signals for the view.
 */
class BookmarkManager : public QObject
{
    Q_OBJECT
public:
    explicit BookmarkManager(QObject *parent = nullptr);

    // Core operations (routed to Rust; Rust decides)
    int toggleBookmark(const QString &filePath, int line, const QString &text = QString());
    void removeBookmark(int id);
    void removeAllBookmarks();
    void removeAllBookmarksForFile(const QString &filePath);

    // Navigation (Rust picks target; Qt moves the caret + emits)
    void nextBookmark();
    void previousBookmark();
    void goToBookmark(int id);
    void navigateTo(int id);

    // Query (Rust answers)
    bool isBookmarked(const QString &filePath, int line) const;
    int bookmarkAt(const QString &filePath, int line) const;
    QList<Bookmark> bookmarks() const;                 // camelCase [] from Rust
    QList<Bookmark> bookmarksForFile(const QString &filePath) const;
    int bookmarkCount() const;

    // Persistence (QSettings holds the JSON; Rust parses/serializes it)
    void saveToSettings();
    void loadFromSettings();

    // Configuration
    void setEditor(QPlainTextEdit *editor) { m_editor = editor; }
    // Track the file path of the editor so navigation can scope by file.
    void setEditorFilePath(const QString &path) { m_editorFilePath = path; }
    QString editorFilePath() const { return m_editorFilePath; }

signals:
    void bookmarkToggled(int id, const QString &filePath, int line, bool added);
    void bookmarkNavigated(int id, const QString &filePath, int line);
    void bookmarksChanged();

private:
    void navigateToBookmark(int id, const QString &filePath, int line);

    QPlainTextEdit *m_editor;
    QString m_editorFilePath;
    int m_currentIndex = -1;
    RustBookmarkStoreAdapter *m_store;  // Rust owns the store; Qt is view-only
};

#endif // BOOKMARKMANAGER_H
