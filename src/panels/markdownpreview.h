#ifndef MARKDOWNPREVIEW_H
#define MARKDOWNPREVIEW_H

#include <QWidget>
#include <QTextBrowser>
#include <QSplitter>
#include <QPushButton>

class QPlainTextEdit;

/**
 * Markdown Live Preview panel with real-time rendered preview.
 * Features:
 * - Side-by-side editor and preview
 * - Live update as you type
 * - GitHub Flavored Markdown support
 * - Syntax highlighting in preview
 * - Export to HTML
 */
class MarkdownPreview : public QWidget
{
    Q_OBJECT
public:
    explicit MarkdownPreview(QWidget *parent = nullptr);

    // Core operations
    void setEditor(QPlainTextEdit *editor);
    void updatePreview();
    void clear();
    
    // Configuration
    void setAutoUpdate(bool enabled);
    bool autoUpdate() const { return m_autoUpdate; }
    
    void setPreviewFontSize(int size);
    int previewFontSize() const { return m_previewFontSize; }

signals:
    void exportRequested(const QString &html);
    void linkClicked(const QString &url);

private slots:
    void onEditorTextChanged();
    void onExportClicked();
    void onRefreshClicked();
    void onZoomIn();
    void onZoomOut();

private:
    void setupUI();
    QString markdownToHtml(const QString &markdown) const;
    QString processInlineMarkdown(const QString &text) const;
    QString processBlockMarkdown(const QString &text) const;
    
    QSplitter *m_splitter;
    QPlainTextEdit *m_editor;
    QTextBrowser *m_preview;
    QPushButton *m_exportButton;
    QPushButton *m_refreshButton;
    QPushButton *m_zoomInButton;
    QPushButton *m_zoomOutButton;
    
    bool m_autoUpdate;
    int m_previewFontSize;
    int m_updateDebounceMs;
};

#endif // MARKDOWNPREVIEW_H
