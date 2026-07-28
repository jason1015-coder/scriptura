#ifndef LARGEFILEHANDLER_H
#define LARGEFILEHANDLER_H

#include <QObject>
#include <QList>
#include <QTextCursor>
#include <QTimer>

class QPlainTextEdit;

/**
 * Handles large files with lazy loading and virtual scrolling.
 * Features:
 * - Lazy block loading (only render visible blocks)
 * - Chunked syntax highlighting
 * - Virtual scrolling for million-line files
 * - Memory-efficient text storage
 */
class LargeFileHandler : public QObject
{
    Q_OBJECT
public:
    explicit LargeFileHandler(QPlainTextEdit *editor, QObject *parent = nullptr);

    // Core operations
    bool loadLargeFile(const QString &filePath);
    void unloadFile();
    bool isLargeFile() const { return m_isLargeFile; }
    
    // Configuration
    void setLargeFileThreshold(int lines);
    int largeFileThreshold() const { return m_threshold; }
    
    void setChunkSize(int lines);
    int chunkSize() const { return m_chunkSize; }
    
    void setHighlightChunkSize(int lines);
    int highlightChunkSize() const { return m_highlightChunkSize; }
    
    // Query
    int totalLines() const { return m_totalLines; }
    int loadedLines() const { return m_loadedLines; }
    double loadProgress() const;
    
    // Virtual scrolling
    void setVisibleRange(int startLine, int endLine);
    void ensureLineLoaded(int line);

signals:
    void fileLoaded(int totalLines);
    void fileUnloaded();
    void loadProgressChanged(double progress);
    void chunkLoaded(int startLine, int endLine);

private slots:
    void onScrollChanged();
    void onLoadChunk();
    
private:
    void setupVirtualScrolling();
    void loadChunk(int startLine, int count);
    void unloadDistantChunks();
    void updateHighlighting();
    
    QPlainTextEdit *m_editor;
    QString m_filePath;
    bool m_isLargeFile;
    int m_threshold;
    int m_chunkSize;
    int m_highlightChunkSize;
    
    // File state
    int m_totalLines;
    int m_loadedLines;
    QList<int> m_loadedChunks;  // Start line of each loaded chunk
    
    // Virtual scrolling state
    int m_visibleStart;
    int m_visibleEnd;
    QTimer *m_scrollTimer;
    QTimer *m_highlightTimer;
    
    // Memory management
    struct TextChunk {
        int startLine;
        int lineCount;
        QStringList lines;
    };
    QList<TextChunk> m_chunks;
};

#endif // LARGEFILEHANDLER_H
