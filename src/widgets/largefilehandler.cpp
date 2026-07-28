#include "largefilehandler.h"
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QScrollBar>
#include <QDebug>

LargeFileHandler::LargeFileHandler(QPlainTextEdit *editor, QObject *parent)
    : QObject(parent)
    , m_editor(editor)
    , m_isLargeFile(false)
    , m_threshold(10000)      // 10K lines threshold
    , m_chunkSize(1000)       // Load 1000 lines at a time
    , m_highlightChunkSize(500)  // Highlight 500 lines at a time
    , m_totalLines(0)
    , m_loadedLines(0)
    , m_visibleStart(0)
    , m_visibleEnd(0)
    , m_scrollTimer(new QTimer(this))
    , m_highlightTimer(new QTimer(this))
{
    m_scrollTimer->setSingleShot(true);
    m_scrollTimer->setInterval(100);
    connect(m_scrollTimer, &QTimer::timeout, this, &LargeFileHandler::onScrollChanged);
    
    m_highlightTimer->setSingleShot(true);
    m_highlightTimer->setInterval(200);
    connect(m_highlightTimer, &QTimer::timeout, this, &LargeFileHandler::onLoadChunk);
}

bool LargeFileHandler::loadLargeFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "LargeFileHandler: Cannot open file:" << filePath;
        return false;
    }
    
    // Count lines first to determine if it's a large file
    QTextStream stream(&file);
    int lineCount = 0;
    while (!stream.atEnd()) {
        stream.readLine();
        lineCount++;
    }
    
    m_totalLines = lineCount;
    m_filePath = filePath;
    
    // Determine if we need virtual scrolling
    m_isLargeFile = (lineCount > m_threshold);
    
    if (m_isLargeFile) {
        qDebug() << "LargeFileHandler: Large file detected with" << lineCount << "lines";
        
        // Load initial chunk
        file.seek(0);
        stream.seek(0);
        loadChunk(0, m_chunkSize);
        
        // Set up virtual scrolling
        setupVirtualScrolling();
    } else {
        // Load entire file normally
        file.seek(0);
        stream.seek(0);
        QString content = stream.readAll();
        m_editor->setPlainText(content);
        m_loadedLines = lineCount;
    }
    
    file.close();
    
    emit fileLoaded(lineCount);
    return true;
}

void LargeFileHandler::unloadFile()
{
    m_editor->clear();
    m_chunks.clear();
    m_loadedChunks.clear();
    m_totalLines = 0;
    m_loadedLines = 0;
    m_isLargeFile = false;
    
    emit fileUnloaded();
}

void LargeFileHandler::setLargeFileThreshold(int lines)
{
    m_threshold = lines;
}

void LargeFileHandler::setChunkSize(int lines)
{
    m_chunkSize = lines;
}

void LargeFileHandler::setHighlightChunkSize(int lines)
{
    m_highlightChunkSize = lines;
}

double LargeFileHandler::loadProgress() const
{
    if (m_totalLines == 0) return 0.0;
    return static_cast<double>(m_loadedLines) / m_totalLines;
}

void LargeFileHandler::setVisibleRange(int startLine, int endLine)
{
    m_visibleStart = startLine;
    m_visibleEnd = endLine;
    
    // Ensure lines in visible range are loaded
    ensureLineLoaded(startLine);
    ensureLineLoaded(endLine);
    
    // Unload distant chunks to save memory
    unloadDistantChunks();
    
    // Schedule highlighting for visible area
    m_highlightTimer->start();
}

void LargeFileHandler::ensureLineLoaded(int line)
{
    if (!m_isLargeFile) return;
    
    // Check if line is already loaded
    for (const TextChunk &chunk : m_chunks) {
        if (line >= chunk.startLine && line < chunk.startLine + chunk.lineCount) {
            return;  // Already loaded
        }
    }
    
    // Load chunk containing this line
    int chunkStart = (line / m_chunkSize) * m_chunkSize;
    loadChunk(chunkStart, m_chunkSize);
}

void LargeFileHandler::setupVirtualScrolling()
{
    if (!m_editor) return;
    
    // Connect to scroll events
    QScrollBar *vbar = m_editor->verticalScrollBar();
    connect(vbar, &QScrollBar::valueChanged, this, [this](int value) {
        Q_UNUSED(value);
        m_scrollTimer->start();
    });
}

void LargeFileHandler::onScrollChanged()
{
    if (!m_editor || !m_isLargeFile) return;
    
    // Calculate visible lines based on scroll position and viewport
    QScrollBar *vbar = m_editor->verticalScrollBar();
    int viewportHeight = m_editor->viewport()->height();
    int lineHeight = m_editor->fontMetrics().height();
    
    int startLine = vbar->value() / lineHeight;
    int endLine = startLine + (viewportHeight / lineHeight) + 10;  // Add buffer
    
    setVisibleRange(startLine, endLine);
}

void LargeFileHandler::loadChunk(int startLine, int count)
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    
    QTextStream stream(&file);
    
    // Skip to start line
    for (int i = 0; i < startLine && !stream.atEnd(); ++i) {
        stream.readLine();
    }
    
    // Read chunk
    TextChunk chunk;
    chunk.startLine = startLine;
    chunk.lineCount = 0;
    
    while (!stream.atEnd() && chunk.lineCount < count) {
        chunk.lines.append(stream.readLine());
        chunk.lineCount++;
    }
    
    file.close();
    
    if (chunk.lineCount > 0) {
        m_chunks.append(chunk);
        m_loadedChunks.append(startLine);
        m_loadedLines += chunk.lineCount;
        
        // Insert into editor (simplified - in production you'd use virtual document)
        QTextCursor cursor(m_editor->document());
        if (startLine == 0) {
            cursor.movePosition(QTextCursor::End);
        } else {
            // Find position for this chunk
            QTextBlock block = m_editor->document()->findBlockByNumber(startLine);
            if (block.isValid()) {
                cursor.setPosition(block.position());
            } else {
                cursor.movePosition(QTextCursor::End);
            }
        }
        
        cursor.insertText(chunk.lines.join('\n') + '\n');
        
        emit chunkLoaded(startLine, startLine + chunk.lineCount);
        emit loadProgressChanged(loadProgress());
    }
}

void LargeFileHandler::unloadDistantChunks()
{
    if (!m_isLargeFile) return;
    
    const int buffer = m_chunkSize * 3;  // Keep chunks within 3x chunk size
    
    for (int i = m_chunks.size() - 1; i >= 0; --i) {
        const TextChunk &chunk = m_chunks[i];
        
        // Check if chunk is far from visible range
        bool farFromVisible = (chunk.startLine + chunk.lineCount < m_visibleStart - buffer) ||
                              (chunk.startLine > m_visibleEnd + buffer);
        
        if (farFromVisible && m_chunks.size() > 3) {  // Keep at least 3 chunks
            m_loadedLines -= chunk.lineCount;
            m_loadedChunks.removeOne(chunk.startLine);
            m_chunks.removeAt(i);
        }
    }
}

void LargeFileHandler::onLoadChunk()
{
    if (!m_isLargeFile) return;
    
    // Highlight visible chunk
    int startLine = (m_visibleStart / m_highlightChunkSize) * m_highlightChunkSize;
    updateHighlighting();
}

void LargeFileHandler::updateHighlighting()
{
    // Trigger syntax highlighting for visible chunks
    // This would integrate with the CodeHighlighter
    // For now, we just emit a signal
    if (m_editor && m_editor->document()) {
        // Force rehighlight of visible area
        m_editor->document()->markContentsDirty(
            m_editor->document()->findBlockByNumber(m_visibleStart).position(),
            m_editor->document()->findBlockByNumber(m_visibleEnd).position() -
            m_editor->document()->findBlockByNumber(m_visibleStart).position()
        );
    }
}
