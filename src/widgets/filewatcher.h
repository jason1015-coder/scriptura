#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QMap>
#include <QString>
#include <QTimer>

/**
 * Represents a watched file with its last known modification time
 */
struct WatchedFile {
    QString filePath;
    qint64 lastModified;
    QByteArray lastContent;
    bool isModified;  // Whether the file was modified externally
};

/**
 * File Watcher monitors files for external changes and prompts for reload.
 * Features:
 * - Auto-detect external modifications
 * - Debounced change notifications
 * - Diff preview before reload
 * - Batch change notifications
 */
class FileWatcher : public QObject
{
    Q_OBJECT
public:
    explicit FileWatcher(QObject *parent = nullptr);
    ~FileWatcher();

    // Core operations
    void watchFile(const QString &filePath);
    void unwatchFile(const QString &filePath);
    void unwatchAllFiles();
    
    // Query
    bool isWatching(const QString &filePath) const;
    QStringList watchedFiles() const { return m_watchedFiles.keys(); }
    bool hasChanges(const QString &filePath) const;
    
    // Manual refresh
    void refreshFile(const QString &filePath);
    
    // Configuration
    void setDebounceInterval(int ms);
    int debounceInterval() const { return m_debounceInterval; }
    
    void setAutoReload(bool enabled);
    bool autoReload() const { return m_autoReload; }

signals:
    void fileChangedExternally(const QString &filePath);
    void fileChangedWithDiff(const QString &filePath, const QString &diff);
    void multipleFilesChanged(const QStringList &filePaths);
    void fileCreated(const QString &filePath);
    void fileDeleted(const QString &filePath);
    void directoryChanged(const QString &dirPath);

private slots:
    void onFileChanged(const QString &path);
    void onDirectoryChanged(const QString &path);
    void processPendingChanges();

private:
    void processFileChange(const QString &filePath);
    QString computeDiff(const QString &oldContent, const QString &newContent) const;
    
    QFileSystemWatcher *m_watcher;
    QMap<QString, WatchedFile> m_watchedFiles;
    QStringList m_pendingChanges;
    QTimer *m_debounceTimer;
    int m_debounceInterval;
    bool m_autoReload;
};

#endif // FILEWATCHER_H
