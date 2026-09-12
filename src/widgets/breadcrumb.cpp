#include "breadcrumb.h"
#include <QPainter>
#include <QMouseEvent>
#include <QTextCursor>
#include <QTextBlock>
#include <QFileInfo>

Breadcrumb::Breadcrumb(QPlainTextEdit *editor, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
{
    setObjectName("breadcrumb");
    setFixedHeight(24);
    setCursor(Qt::PointingHandCursor);
}

void Breadcrumb::setFilePath(const QString &path)
{
    m_filePath = path;
    update();
}

void Breadcrumb::setSymbolPath(const QString &path)
{
    m_symbolPath = path;
    update();
}

void Breadcrumb::updateFromCursor()
{
    if (!m_editor) return;
    
    QTextCursor cursor = m_editor->textCursor();
    QTextBlock block = cursor.block();

    QString lineText = block.text().trimmed();
    if (!lineText.isEmpty()) {
        m_symbolPath = lineText.left(30);
    } else {
        m_symbolPath.clear();
    }
    
    update();
}

void Breadcrumb::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), palette().color(QPalette::Window));
    
    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);
    
    int x = 10;
    int y = height() / 2 + fontMetrics().height() / 4;
    
    QStringList parts = parseFilePath(m_filePath);
    for (int i = 0; i < parts.size(); ++i) {
        QString part = parts[i];
        int width = fontMetrics().horizontalAdvance(part);
        
        if (i > 0) {
            painter.setPen(palette().color(QPalette::Text));
            painter.drawText(x, y, " > ");
            x += fontMetrics().horizontalAdvance(" > ");
        }
        
        painter.setPen(palette().color(QPalette::Link));
        painter.drawText(x, y, part);
        x += width;
    }
    
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(x + 10, y, "|");
    x += fontMetrics().horizontalAdvance("|") + 20;
    
    if (!m_symbolPath.isEmpty()) {
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(x, y, m_symbolPath);
    }
}

void Breadcrumb::mousePressEvent(QMouseEvent *event)
{
    int index = hitTest(event->pos());
    if (index >= 0) {
        QStringList parts = parseFilePath(m_filePath);
        if (index < parts.size()) {
            QString path;
            for (int i = 0; i <= index; ++i) {
                path += "/" + parts[i];
            }
            emit breadcrumbClicked(path);
        } else {
            emit breadcrumbClicked(m_symbolPath);
        }
    }
}

QStringList Breadcrumb::parseFilePath(const QString &path) const
{
    QStringList parts;
    if (path.isEmpty()) return parts;
    
    QFileInfo info(path);
    parts = info.absolutePath().split('/', Qt::SkipEmptyParts);
    parts.append(info.fileName());
    
    if (parts.size() > 4) {
        parts = parts.mid(parts.size() - 4);
    }
    
    return parts;
}

int Breadcrumb::hitTest(const QPoint &pos) const
{
    QStringList parts = parseFilePath(m_filePath);
    QFont font = this->font();
    font.setPointSize(9);
    QFontMetrics fm(font);
    
    int x = 10;
    for (int i = 0; i < parts.size(); ++i) {
        int width = fm.horizontalAdvance(parts[i]);
        
        if (i > 0) {
            x += fm.horizontalAdvance(" > ");
        }
        
        if (pos.x() >= x && pos.x() <= x + width) {
            return i;
        }

        x += width;
    }

    x += fm.horizontalAdvance(" > ");

    if (!m_symbolPath.isEmpty()) {
        int symWidth = fm.horizontalAdvance(m_symbolPath);
        int sepWidth = fm.horizontalAdvance("|");
        x += sepWidth + 10;
        if (pos.x() >= x && pos.x() <= x + symWidth) {
            return parts.size();
        }
    }

    return -1;
}
