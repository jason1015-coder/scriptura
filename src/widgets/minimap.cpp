#include "minimap.h"
#include <QPainter>
#include <QTextBlock>
#include <QScrollBar>
#include <QMouseEvent>
#include <QTextDocument>

Minimap::Minimap(QPlainTextEdit *editor, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
    , m_document(nullptr)
{
    setObjectName("minimap");
    setFixedWidth(80);
    
    if (m_editor) {
        setDocument(m_editor->document());
        connect(m_editor, &QPlainTextEdit::updateRequest, this, &Minimap::updateGeometry);
    }
}

void Minimap::setDocument(QTextDocument *document)
{
    if (m_document) {
        disconnect(m_document, &QTextDocument::contentsChange, this, &Minimap::onDocumentChanged);
    }
    
    m_document = document;
    
    if (m_document) {
        connect(m_document, &QTextDocument::contentsChange, this, &Minimap::onDocumentChanged);
    }
    
    update();
}

void Minimap::setVisible(bool visible)
{
    QWidget::setVisible(visible);
    updateGeometry();
}

void Minimap::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), palette().color(QPalette::AlternateBase));
    
    if (!m_document) return;
    
    QFont font = painter.font();
    font.setPointSize(2);
    painter.setFont(font);
    
    QTextBlock block = m_document->begin();
    int y = 0;
    const int lineHeight = 3;
    
    while (block.isValid() && y < height()) {
        QString text = block.text().trimmed();
        if (!text.isEmpty()) {
            painter.setPen(palette().color(QPalette::Text));
            painter.drawText(0, y, width(), lineHeight, Qt::AlignLeft, text.left(10));
        }
        y += lineHeight;
        block = block.next();
    }
    
    // Draw visible region highlight
    if (m_editor) {
        QScrollBar *vbar = m_editor->verticalScrollBar();
        int viewportHeight = m_editor->viewport()->height();
        int totalHeight = m_editor->document()->size().height();
        
        if (totalHeight > 0) {
            int visibleHeight = (viewportHeight * height()) / totalHeight;
            int visibleY = (vbar->value() * height()) / totalHeight;
            
            painter.setBrush(QColor(100, 100, 100, 100));
            painter.setPen(Qt::NoPen);
            painter.drawRect(0, visibleY, width(), visibleHeight);
        }
    }
}

void Minimap::mousePressEvent(QMouseEvent *event)
{
    if (!m_editor || !m_document) return;
    
    int totalHeight = m_editor->document()->size().height();
    if (totalHeight > 0) {
        int newPosition = static_cast<int>((event->position().y() * totalHeight) / height());
        m_editor->verticalScrollBar()->setValue(newPosition);
    }
}

void Minimap::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        mousePressEvent(event);
    }
}

void Minimap::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

void Minimap::updateGeometry()
{
    if (m_editor) {
        int totalHeight = m_editor->document()->size().height();
        setMinimumHeight(qMin(totalHeight / SCALE_FACTOR, 2000));
    }
    update();
}

void Minimap::onDocumentChanged()
{
    update();
    updateGeometry();
}
