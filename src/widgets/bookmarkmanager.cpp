#include "bookmarkmanager.h"
#include "rust_adapter.h"
#include <QSettings>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextBlock>

BookmarkManager::BookmarkManager(QObject *parent)
    : QObject(parent)
    , m_editor(nullptr)
    , m_store(new RustBookmarkStoreAdapter(this))
{
    loadFromSettings();
}

int BookmarkManager::toggleBookmark(const QString &filePath, int line, const QString &text)
{
    // Rust decides toggle (add or remove) and the returned id.
    const int id = m_store->toggle(filePath, static_cast<uint32_t>(line), text);
    emit bookmarkToggled(id, filePath, line, id >= 0);
    emit bookmarksChanged();
    return id;
}

void BookmarkManager::removeBookmark(int id)
{
    if (m_store->remove(id))
        emit bookmarksChanged();
}

void BookmarkManager::removeAllBookmarks()
{
    if (bookmarkCount() == 0) return;
    m_store->clear();
    emit bookmarksChanged();
}

void BookmarkManager::removeAllBookmarksForFile(const QString &filePath)
{
    if (bookmarksForFile(filePath).isEmpty()) return;
    m_store->clearFile(filePath);
    emit bookmarksChanged();
}

void BookmarkManager::nextBookmark()
{
    if (bookmarkCount() == 0) return;
    int currentLine = -1;
    if (m_editor) currentLine = m_editor->textCursor().blockNumber();
    RustBookmarkStoreAdapter::Nav nav;
    if (m_store->next(m_editorFilePath, currentLine, &nav))
        navigateToBookmark(nav.id, nav.file, nav.line);
}

void BookmarkManager::previousBookmark()
{
    if (bookmarkCount() == 0) return;
    int currentLine = 0;
    if (m_editor) currentLine = m_editor->textCursor().blockNumber();
    RustBookmarkStoreAdapter::Nav nav;
    if (m_store->prev(m_editorFilePath, currentLine, &nav))
        navigateToBookmark(nav.id, nav.file, nav.line);
}

void BookmarkManager::goToBookmark(int id)
{
    navigateTo(id);
}

void BookmarkManager::navigateTo(int id)
{
    // Rust answers the lookup; Qt moves the caret and emits the signal.
    const auto all = bookmarks();
    for (const Bookmark &bm : all) {
        if (bm.id != id) continue;
        navigateToBookmark(bm.id, bm.filePath, bm.line);
        return;
    }
}

void BookmarkManager::navigateToBookmark(int id, const QString &filePath, int line)
{
    if (m_editor) {
        QTextBlock block = m_editor->document()->findBlockByNumber(line);
        if (block.isValid()) {
            QTextCursor cursor(block);
            cursor.movePosition(QTextCursor::StartOfBlock);
            m_editor->setTextCursor(cursor);
        }
    }
    emit bookmarkNavigated(id, filePath, line);
}

bool BookmarkManager::isBookmarked(const QString &filePath, int line) const
{
    return m_store->isBookmarked(filePath, static_cast<uint32_t>(line));
}

int BookmarkManager::bookmarkAt(const QString &filePath, int line) const
{
    return m_store->bookmarkAt(filePath, static_cast<uint32_t>(line));
}

QList<Bookmark> BookmarkManager::bookmarks() const
{
    QList<Bookmark> result;
    const QJsonDocument doc = QJsonDocument::fromJson(m_store->toJson().toUtf8());
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        Bookmark bm;
        bm.id = o.value("id").toInt(-1);
        bm.filePath = o.value("file").toString();
        bm.line = o.value("line").toInt(-1);
        bm.text = o.value("text").toString();
        if (bm.id >= 0) result.append(bm);
    }
    return result;
}

QList<Bookmark> BookmarkManager::bookmarksForFile(const QString &filePath) const
{
    QList<Bookmark> result;
    for (const Bookmark &bm : bookmarks())
        if (bm.filePath == filePath) result.append(bm);
    return result;
}

int BookmarkManager::bookmarkCount() const
{
    return bookmarks().size();
}

void BookmarkManager::saveToSettings()
{
    QSettings settings;
    settings.setValue("bookmarks", m_store->toQtJson().toUtf8());
}

void BookmarkManager::loadFromSettings()
{
    QSettings settings;
    const QByteArray data = settings.value("bookmarks").toByteArray();
    if (data.isEmpty()) {
        m_store->clear();
        return;
    }
    m_store->loadQtJson(QString::fromUtf8(data));
}
