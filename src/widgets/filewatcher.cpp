#include "filewatcher.h"
#include "rust_adapter.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QDir>

FileWatcher::FileWatcher(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
    , m_debounceTimer(new QTimer(this))
    , m_debounceInterval(500)
    , m_autoReload(false)
{
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &FileWatcher::onFileChanged);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &FileWatcher::onDirectoryChanged);
    
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(m_debounceInterval);
    connect(m_debounceTimer, &QTimer::timeout,
            this, &FileWatcher::processPendingChanges);
}

FileWatcher::~FileWatcher()
{
}

void FileWatcher::watchFile(const QString &filePath)
{
    if (m_watchedFiles.contains(filePath)) {
        return;
    }
    
    QFile file(filePath);
    if (!file.exists()) {
        return;
    }
    
    WatchedFile watched;
    watched.filePath = filePath;
    watched.lastModified = QFileInfo(filePath).lastModified().toMSecsSinceEpoch();
    
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        watched.lastContent = file.readAll();
        file.close();
    }
    
    watched.isModified = false;
    m_watchedFiles.insert(filePath, watched);
    
    // Watch the file
    m_watcher->addPath(filePath);
    
    // Also watch the parent directory for new/deleted files
    QString dirPath = QFileInfo(filePath).absolutePath();
    if (!m_watcher->directories().contains(dirPath)) {
        m_watcher->addPath(dirPath);
    }
}

void FileWatcher::unwatchFile(const QString &filePath)
{
    if (!m_watchedFiles.contains(filePath)) {
        return;
    }
    
    m_watcher->removePath(filePath);
    m_watchedFiles.remove(filePath);
    
    // Check if we still need to watch the parent directory
    QString dirPath = QFileInfo(filePath).absolutePath();
    bool dirStillNeeded = false;
    for (const QString &watched : m_watchedFiles.keys()) {
        if (QFileInfo(watched).absolutePath() == dirPath) {
            dirStillNeeded = true;
            break;
        }
    }
    
    if (!dirStillNeeded) {
        m_watcher->removePath(dirPath);
    }
}

void FileWatcher::unwatchAllFiles()
{
    m_watcher->removePaths(m_watcher->files());
    m_watcher->removePaths(m_watcher->directories());
    m_watchedFiles.clear();
}

bool FileWatcher::isWatching(const QString &filePath) const
{
    return m_watchedFiles.contains(filePath);
}

bool FileWatcher::hasChanges(const QString &filePath) const
{
    if (!m_watchedFiles.contains(filePath)) {
        return false;
    }
    return m_watchedFiles[filePath].isModified;
}

void FileWatcher::refreshFile(const QString &filePath)
{
    if (!m_watchedFiles.contains(filePath)) {
        return;
    }
    
    QFile file(filePath);
    if (!file.exists()) {
        return;
    }
    
    WatchedFile &watched = m_watchedFiles[filePath];
    watched.lastModified = QFileInfo(filePath).lastModified().toMSecsSinceEpoch();
    
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        watched.lastContent = file.readAll();
        file.close();
    }
    
    watched.isModified = false;
}

void FileWatcher::setDebounceInterval(int ms)
{
    m_debounceInterval = ms;
    m_debounceTimer->setInterval(m_debounceInterval);
}

void FileWatcher::setAutoReload(bool enabled)
{
    m_autoReload = enabled;
}

void FileWatcher::onFileChanged(const QString &path)
{
    // The file might have been removed and re-added
    if (!QFile::exists(path)) {
        // File was deleted
        emit fileDeleted(path);
        return;
    }
    
    // Check if we're already tracking this file
    if (m_watchedFiles.contains(path)) {
        processFileChange(path);
    } else {
        // New file created
        emit fileCreated(path);
    }
}

void FileWatcher::onDirectoryChanged(const QString &path)
{
    emit directoryChanged(path);
    
    // Check for new files in the directory
    QDir dir(path);
    QStringList files = dir.entryList(QDir::Files);
    
    for (const QString &file : files) {
        QString fullPath = dir.absoluteFilePath(file);
        if (!m_watchedFiles.contains(fullPath)) {
            // Check if this is a file we should be watching
            // (e.g., if it was just created)
            emit fileCreated(fullPath);
        }
    }
}

void FileWatcher::processFileChange(const QString &filePath)
{
    if (!m_watchedFiles.contains(filePath)) {
        return;
    }
    
    WatchedFile &watched = m_watchedFiles[filePath];
    
    // Read current content
    QFile file(filePath);
    QByteArray currentContent;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        currentContent = file.readAll();
        file.close();
    }
    
    // Check if content actually changed
    if (currentContent == watched.lastContent) {
        return;
    }
    
    // Mark as modified
    watched.isModified = true;
    
    // Compute diff
    QString diff = computeDiff(watched.lastContent, currentContent);
    
    // Update last known state
    watched.lastContent = currentContent;
    watched.lastModified = QFileInfo(filePath).lastModified().toMSecsSinceEpoch();
    
    // Emit change notification
    emit fileChangedExternally(filePath);
    if (!diff.isEmpty()) {
        emit fileChangedWithDiff(filePath, diff);
    }
    
    // Add to pending changes for batch processing
    if (!m_pendingChanges.contains(filePath)) {
        m_pendingChanges.append(filePath);
    }
    
    // Start debounce timer
    m_debounceTimer->start();
}

void FileWatcher::processPendingChanges()
{
    if (m_pendingChanges.isEmpty()) {
        return;
    }
    
    QStringList changed = m_pendingChanges;
    m_pendingChanges.clear();
    
    if (changed.size() == 1) {
        emit fileChangedExternally(changed.first());
    } else {
        emit multipleFilesChanged(changed);
    }
}

QString FileWatcher::computeDiff(const QString &oldContent, const QString &newContent) const
{
    // Delegate LCS diff computation to the Rust backend.
    QByteArray leftBytes = oldContent.toUtf8();
    QByteArray rightBytes = newContent.toUtf8();
    char *json = rust_diff_compute(leftBytes.constData(), rightBytes.constData());
    if (!json) return QString();

    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json));
    rust_free_string(json);
    if (!doc.isArray()) return QString();

    QString diff;
    for (const QJsonValue &v : doc.array()) {
        QJsonObject hunk = v.toObject();
        int leftStart = hunk["leftStart"].toInt();
        int leftCount = hunk["leftCount"].toInt();
        int rightStart = hunk["rightStart"].toInt();
        int rightCount = hunk["rightCount"].toInt();
        diff += QString("@@ -%1,%2 +%3,%4 @@\n")
                    .arg(leftStart + 1).arg(leftCount)
                    .arg(rightStart + 1).arg(rightCount);
        const QJsonArray lines = hunk["lines"].toArray();
        for (const QJsonValue &l : lines) {
            QJsonObject line = l.toObject();
            const QString type = line["type"].toString();
            const QString text = line["text"].toString();
            if (type == "removed" && !text.isEmpty()) {
                diff += "- " + text + "\n";
            } else if (type == "added" && !text.isEmpty()) {
                diff += "+ " + text + "\n";
            }
        }
        diff += "\n";
    }
    return diff;
}
