#include "bookmarkmanager.h"
#include "internals/settings_store.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextBlock>

BookmarkManager::BookmarkManager(QObject *parent)
    : QObject(parent)
    , m_editor(nullptr)
    , m_nextId(1)
    , m_currentIndex(-1)
{
    loadFromSettings();
}

int BookmarkManager::toggleBookmark(const QString &filePath, int line, const QString &text)
{
    // Check if already bookmarked
    int existingId = bookmarkAt(filePath, line);
    if (existingId >= 0) {
        removeBookmark(existingId);
        return -1;
    }
    
    // Add new bookmark
    Bookmark bm;
    bm.filePath = filePath;
    bm.line = line;
    bm.text = text;
    bm.id = m_nextId++;
    m_bookmarks.append(bm);
    
    saveToSettings();
    emit bookmarkToggled(bm.id, filePath, line, true);
    emit bookmarksChanged();
    
    return bm.id;
}

void BookmarkManager::removeBookmark(int id)
{
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        if (m_bookmarks[i].id == id) {
            Bookmark bm = m_bookmarks.takeAt(i);
            saveToSettings();
            emit bookmarkToggled(id, bm.filePath, bm.line, false);
            emit bookmarksChanged();
            return;
        }
    }
}

void BookmarkManager::removeAllBookmarks()
{
    if (m_bookmarks.isEmpty()) return;
    
    m_bookmarks.clear();
    saveToSettings();
    emit bookmarksChanged();
}

void BookmarkManager::removeAllBookmarksForFile(const QString &filePath)
{
    bool changed = false;
    for (int i = m_bookmarks.size() - 1; i >= 0; --i) {
        if (m_bookmarks[i].filePath == filePath) {
            Bookmark bm = m_bookmarks.takeAt(i);
            emit bookmarkToggled(bm.id, bm.filePath, bm.line, false);
            changed = true;
        }
    }
    
    if (changed) {
        saveToSettings();
        emit bookmarksChanged();
    }
}

void BookmarkManager::nextBookmark()
{
    if (m_bookmarks.isEmpty()) return;

    int currentLine = -1;
    if (m_editor) {
        currentLine = m_editor->textCursor().blockNumber();
    }

    int nextIdx = findNextBookmarkIndex(currentLine, m_editorFilePath);
    if (nextIdx >= 0) {
        m_currentIndex = nextIdx;
        const Bookmark &bm = m_bookmarks[nextIdx];
        emit bookmarkNavigated(bm.id, bm.filePath, bm.line);
    }
}

void BookmarkManager::previousBookmark()
{
    if (m_bookmarks.isEmpty()) return;

    int currentLine = 0;
    if (m_editor) {
        currentLine = m_editor->textCursor().blockNumber();
    }

    int prevIdx = findPreviousBookmarkIndex(currentLine, m_editorFilePath);
    if (prevIdx >= 0) {
        m_currentIndex = prevIdx;
        const Bookmark &bm = m_bookmarks[prevIdx];
        emit bookmarkNavigated(bm.id, bm.filePath, bm.line);
    }
}

void BookmarkManager::goToBookmark(int id)
{
    navigateTo(id);
}

void BookmarkManager::navigateTo(int id)
{
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        if (m_bookmarks[i].id != id)
            continue;

        m_currentIndex = i;
        const Bookmark &bm = m_bookmarks[i];

        // Move the caret when the editor is available so the bookmark is
        // actually visible to the user, not just signalled.
        if (m_editor) {
            QTextBlock block = m_editor->document()->findBlockByNumber(bm.line);
            if (block.isValid()) {
                QTextCursor cursor(block);
                cursor.movePosition(QTextCursor::StartOfBlock);
                m_editor->setTextCursor(cursor);
            }
        }

        emit bookmarkNavigated(bm.id, bm.filePath, bm.line);
        return;
    }
}

bool BookmarkManager::isBookmarked(const QString &filePath, int line) const
{
    return bookmarkAt(filePath, line) >= 0;
}

int BookmarkManager::bookmarkAt(const QString &filePath, int line) const
{
    for (const Bookmark &bm : m_bookmarks) {
        if (bm.filePath == filePath && bm.line == line) {
            return bm.id;
        }
    }
    return -1;
}

QList<Bookmark> BookmarkManager::bookmarksForFile(const QString &filePath) const
{
    QList<Bookmark> result;
    for (const Bookmark &bm : m_bookmarks) {
        if (bm.filePath == filePath) {
            result.append(bm);
        }
    }
    return result;
}

void BookmarkManager::saveToSettings()
{
    QJsonArray arr;

    for (const Bookmark &bm : m_bookmarks) {
        QJsonObject obj;
        obj["filePath"] = bm.filePath;
        obj["line"] = bm.line;
        obj["text"] = bm.text;
        obj["id"] = bm.id;
        arr.append(obj);
    }

    SettingsStore::instance().set("bookmarks", QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void BookmarkManager::loadFromSettings()
{
    QString data = SettingsStore::instance().get("bookmarks");

    if (data.isEmpty()) return;

    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    QJsonArray arr = doc.array();

    m_bookmarks.clear();
    m_nextId = 1;

    for (const QJsonValue &v : arr) {
        QJsonObject obj = v.toObject();
        Bookmark bm;
        bm.filePath = obj["filePath"].toString();
        bm.line = obj["line"].toInt();
        bm.text = obj["text"].toString();
        bm.id = obj["id"].toInt();
        m_bookmarks.append(bm);

        if (bm.id >= m_nextId) {
            m_nextId = bm.id + 1;
        }
    }
}

int BookmarkManager::generateId() const
{
    return m_nextId;
}

int BookmarkManager::findNextBookmarkIndex(int currentLine, const QString &filePath) const
{
    if (m_bookmarks.isEmpty()) return -1;

    // First pass: prefer bookmarks in the same file, strictly after the
    // current line. Return the first match.
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        if (m_bookmarks[i].filePath == filePath && m_bookmarks[i].line > currentLine)
            return i;
    }

    // Second pass: same file, wrap to the first bookmark of that file.
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        if (m_bookmarks[i].filePath == filePath)
            return i;
    }

    // Third pass: fall back to any bookmark (different file).
    if (!m_bookmarks.isEmpty())
        return 0;

    return -1;
}

int BookmarkManager::findPreviousBookmarkIndex(int currentLine, const QString &filePath) const
{
    if (m_bookmarks.isEmpty()) return -1;

    // First pass: prefer bookmarks in the same file, strictly before the
    // current line. Return the last match.
    int result = -1;
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        if (m_bookmarks[i].filePath == filePath && m_bookmarks[i].line < currentLine)
            result = i;
    }
    if (result >= 0)
        return result;

    // Second pass: same file, wrap to the last bookmark of that file.
    for (int i = m_bookmarks.size() - 1; i >= 0; --i) {
        if (m_bookmarks[i].filePath == filePath)
            return i;
    }

    // Third pass: fall back to the last bookmark overall.
    return m_bookmarks.size() - 1;
}
