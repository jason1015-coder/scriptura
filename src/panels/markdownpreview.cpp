#include "markdownpreview.h"
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QApplication>
#include <QRegularExpression>

MarkdownPreview::MarkdownPreview(QWidget *parent)
    : QWidget(parent)
    , m_editor(nullptr)
    , m_autoUpdate(true)
    , m_previewFontSize(14)
    , m_updateDebounceMs(300)
{
    setupUI();
    
    // Connect editor text changes with debounce
    QTimer *debounceTimer = new QTimer(this);
    debounceTimer->setSingleShot(true);
    debounceTimer->setInterval(m_updateDebounceMs);
    
    connect(m_exportButton, &QPushButton::clicked, this, &MarkdownPreview::onExportClicked);
    connect(m_refreshButton, &QPushButton::clicked, this, &MarkdownPreview::onRefreshClicked);
    connect(m_zoomInButton, &QPushButton::clicked, this, &MarkdownPreview::onZoomIn);
    connect(m_zoomOutButton, &QPushButton::clicked, this, &MarkdownPreview::onZoomOut);
    connect(m_preview, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
        emit linkClicked(url.toString());
    });
}

void MarkdownPreview::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Toolbar
    QWidget *toolbar = new QWidget(this);
    toolbar->setStyleSheet("background: palette(window); border-bottom: 1px solid palette(mid);");
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(8, 4, 8, 4);
    
    QLabel *titleLabel = new QLabel(tr("Markdown Preview"), this);
    titleLabel->setStyleSheet("font-weight: bold;");
    m_refreshButton = new QPushButton(tr("↻"), this);
    m_refreshButton->setToolTip(tr("Refresh preview"));
    m_refreshButton->setFixedSize(24, 24);
    m_zoomInButton = new QPushButton(tr("+"), this);
    m_zoomInButton->setToolTip(tr("Zoom in"));
    m_zoomInButton->setFixedSize(24, 24);
    m_zoomOutButton = new QPushButton(tr("-"), this);
    m_zoomOutButton->setToolTip(tr("Zoom out"));
    m_zoomOutButton->setFixedSize(24, 24);
    m_exportButton = new QPushButton(tr("Export HTML"), this);
    
    toolbarLayout->addWidget(titleLabel);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_zoomOutButton);
    toolbarLayout->addWidget(m_zoomInButton);
    toolbarLayout->addWidget(m_refreshButton);
    toolbarLayout->addWidget(m_exportButton);
    
    mainLayout->addWidget(toolbar);
    
    // Splitter for editor and preview
    m_splitter = new QSplitter(Qt::Horizontal, this);
    
    // Preview (no editor in this implementation - preview only)
    m_preview = new QTextBrowser(this);
    m_preview->setOpenExternalLinks(true);
    m_preview->setStyleSheet(R"(
        QTextBrowser {
            background: palette(base);
            color: palette(text);
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            font-size: 14px;
            padding: 16px;
            border: none;
        }
    )");
    
    m_splitter->addWidget(m_preview);
    m_splitter->setStretchFactor(0, 1);
    
    mainLayout->addWidget(m_splitter, 1);
}

void MarkdownPreview::setEditor(QPlainTextEdit *editor)
{
    if (m_editor) {
        disconnect(m_editor, nullptr, this, nullptr);
    }
    
    m_editor = editor;
    
    if (m_editor) {
        connect(m_editor, &QPlainTextEdit::textChanged,
                this, &MarkdownPreview::onEditorTextChanged);
        updatePreview();
    }
}

void MarkdownPreview::updatePreview()
{
    if (!m_editor) return;
    
    QString markdown = m_editor->toPlainText();
    QString html = markdownToHtml(markdown);
    
    // Wrap in full HTML with styling
    QString fullHtml = QString(R"(
        <!DOCTYPE html>
        <html>
        <head>
            <style>
                body {
                    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
                    font-size: %1px;
                    line-height: 1.6;
                    color: %2;
                    background: %3;
                    max-width: 800px;
                    margin: 0 auto;
                    padding: 20px;
                }
                h1, h2, h3, h4, h5, h6 {
                    color: %2;
                    margin-top: 24px;
                    margin-bottom: 16px;
                }
                h1 { font-size: 2em; border-bottom: 1px solid %4; padding-bottom: 0.3em; }
                h2 { font-size: 1.5em; border-bottom: 1px solid %4; padding-bottom: 0.3em; }
                h3 { font-size: 1.25em; }
                code {
                    background: %5;
                    padding: 2px 6px;
                    border-radius: 3px;
                    font-family: "SF Mono", "Monaco", "Inconsolata", "Fira Code", monospace;
                }
                pre {
                    background: %5;
                    padding: 16px;
                    border-radius: 6px;
                    overflow-x: auto;
                }
                pre code {
                    background: none;
                    padding: 0;
                }
                blockquote {
                    border-left: 4px solid %6;
                    margin: 0;
                    padding: 0 16px;
                    color: %7;
                }
                a { color: %6; }
                img { max-width: 100%; }
                table {
                    border-collapse: collapse;
                    width: 100%%;
                }
                th, td {
                    border: 1px solid %4;
                    padding: 8px 12px;
                    text-align: left;
                }
                th { background: %5; }
                ul, ol { padding-left: 2em; }
                li { margin: 4px 0; }
            </style>
        </head>
        <body>%8</body>
        </html>
    )")
    .arg(m_previewFontSize)
    .arg(palette().color(QPalette::Text).name())
    .arg(palette().color(QPalette::Base).name())
    .arg(palette().color(QPalette::Mid).name())
    .arg(palette().color(QPalette::AlternateBase).name())
    .arg(palette().color(QPalette::Highlight).name())
    .arg(palette().color(QPalette::Mid).name())
    .arg(html);
    
    m_preview->setHtml(fullHtml);
}

void MarkdownPreview::clear()
{
    m_preview->clear();
}

void MarkdownPreview::setAutoUpdate(bool enabled)
{
    m_autoUpdate = enabled;
}

void MarkdownPreview::setPreviewFontSize(int size)
{
    m_previewFontSize = size;
    updatePreview();
}

void MarkdownPreview::onEditorTextChanged()
{
    if (m_autoUpdate) {
        updatePreview();
    }
}

void MarkdownPreview::onExportClicked()
{
    QString html = m_preview->toHtml();
    
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Export HTML"),
        QString(),
        tr("HTML Files (*.html);;All Files (*)")
    );
    
    if (filePath.isEmpty()) return;
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << html;
        file.close();
        emit exportRequested(html);
    }
}

void MarkdownPreview::onRefreshClicked()
{
    updatePreview();
}

void MarkdownPreview::onZoomIn()
{
    m_previewFontSize = qMin(m_previewFontSize + 2, 32);
    updatePreview();
}

void MarkdownPreview::onZoomOut()
{
    m_previewFontSize = qMax(m_previewFontSize - 2, 8);
    updatePreview();
}

QString MarkdownPreview::markdownToHtml(const QString &markdown) const
{
    // Simple markdown to HTML conversion
    // In production, use a proper markdown library like cmark or discount
    
    QStringList lines = markdown.split('\n');
    QStringList htmlLines;
    bool inCodeBlock = false;
    bool inList = false;
    bool inBlockquote = false;
    
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        
        // Code blocks
        if (line.trimmed().startsWith("```")) {
            if (inCodeBlock) {
                htmlLines.append("</code></pre>");
                inCodeBlock = false;
            } else {
                QString lang = line.trimmed().mid(3).trimmed();
                htmlLines.append(QString("<pre><code class=\"%1\">").arg(lang));
                inCodeBlock = true;
            }
            continue;
        }
        
        if (inCodeBlock) {
            htmlLines.append(line.toHtmlEscaped());
            continue;
        }
        
        // Empty line
        if (line.trimmed().isEmpty()) {
            if (inList) {
                htmlLines.append("</ul>");
                inList = false;
            }
            if (inBlockquote) {
                htmlLines.append("</blockquote>");
                inBlockquote = false;
            }
            htmlLines.append("");
            continue;
        }
        
        // Headers
        if (line.startsWith("# ")) {
            htmlLines.append("<h1>" + processInlineMarkdown(line.mid(2)) + "</h1>");
        } else if (line.startsWith("## ")) {
            htmlLines.append("<h2>" + processInlineMarkdown(line.mid(3)) + "</h2>");
        } else if (line.startsWith("### ")) {
            htmlLines.append("<h3>" + processInlineMarkdown(line.mid(4)) + "</h3>");
        } else if (line.startsWith("#### ")) {
            htmlLines.append("<h4>" + processInlineMarkdown(line.mid(5)) + "</h4>");
        } else if (line.startsWith("##### ")) {
            htmlLines.append("<h5>" + processInlineMarkdown(line.mid(6)) + "</h5>");
        } else if (line.startsWith("###### ")) {
            htmlLines.append("<h6>" + processInlineMarkdown(line.mid(7)) + "</h6>");
        }
        // Blockquote
        else if (line.startsWith("> ")) {
            if (!inBlockquote) {
                htmlLines.append("<blockquote>");
                inBlockquote = true;
            }
            htmlLines.append(processInlineMarkdown(line.mid(2)));
        }
        // Unordered list
        else if (line.startsWith("- ") || line.startsWith("* ")) {
            if (!inList) {
                htmlLines.append("<ul>");
                inList = true;
            }
            htmlLines.append("<li>" + processInlineMarkdown(line.mid(2)) + "</li>");
        }
        // Ordered list
        else if (QRegularExpression("^\\d+\\.\\s").match(line).hasMatch()) {
            if (!inList) {
                htmlLines.append("<ul>");
                inList = true;
            }
            QString content = line;
            content.remove(QRegularExpression("^\\d+\\.\\s"));
            htmlLines.append("<li>" + processInlineMarkdown(content) + "</li>");
        }
        // Horizontal rule
        else if (line.trimmed() == "---" || line.trimmed() == "***") {
            htmlLines.append("<hr>");
        }
        // Table
        else if (line.contains("|")) {
            // Simple table support
            htmlLines.append("<p>" + processInlineMarkdown(line) + "</p>");
        }
        // Paragraph
        else {
            htmlLines.append("<p>" + processInlineMarkdown(line) + "</p>");
        }
    }
    
    if (inList) {
        htmlLines.append("</ul>");
    }
    if (inBlockquote) {
        htmlLines.append("</blockquote>");
    }
    
    return htmlLines.join("\n");
}

QString MarkdownPreview::processInlineMarkdown(const QString &text) const
{
    QString result = text;
    
    // Escape HTML first
    result = result.toHtmlEscaped();
    
    // Bold
    result.replace(QRegularExpression("\\*\\*(.+?)\\*\\*"), "<strong>\\1</strong>");
    result.replace(QRegularExpression("__(.+?)__"), "<strong>\\1</strong>");
    
    // Italic
    result.replace(QRegularExpression("\\*(.+?)\\*"), "<em>\\1</em>");
    result.replace(QRegularExpression("_(.+?)_"), "<em>\\1</em>");
    
    // Strikethrough
    result.replace(QRegularExpression("~~(.+?)~~"), "<del>\\1</del>");
    
    // Inline code
    result.replace(QRegularExpression("`([^`]+)`"), "<code>\\1</code>");
    
    // Links
    result.replace(QRegularExpression("\\[([^\\]]+)\\]\\(([^)]+)\\)"),
                   "<a href=\"\\2\">\\1</a>");
    
    // Images
    result.replace(QRegularExpression("!\\[([^\\]]*)\\]\\(([^)]+)\\)"),
                   "<img src=\"\\2\" alt=\"\\1\">");
    
    // Line breaks
    result.replace("  \n", "<br>");
    
    return result;
}
