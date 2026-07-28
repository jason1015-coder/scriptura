#include "breadcrumbbar.h"
#include "codeeditor.h"
#include "cssbreadcrumb.h"
#include <QFileInfo>
#include <QDir>
#include <QScrollBar>

BreadcrumbBarWidget::BreadcrumbBarWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void BreadcrumbBarWidget::setupUI()
{
    setObjectName("breadcrumbBar");
    setFixedHeight(24);
    setStyleSheet("QWidget#breadcrumbBar { background-color: transparent; border-bottom: 1px solid palette(mid); }");

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 2, 8, 2);
    mainLayout->setSpacing(0);

    // File icon label
    m_fileLabel = new QLabel(this);
    m_fileLabel->setPixmap(QIcon(":/icons/file-tree.svg").pixmap(12, 12));
    m_fileLabel->setFixedSize(16, 16);
    mainLayout->addWidget(m_fileLabel);

    // Scroll area for segments
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setFixedHeight(22);

    QWidget *scrollContent = new QWidget();
    m_segmentsLayout = new QHBoxLayout(scrollContent);
    m_segmentsLayout->setContentsMargins(4, 0, 0, 0);
    m_segmentsLayout->setSpacing(0);
    m_segmentsLayout->addStretch();
    m_scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(m_scrollArea, 1);
}

void BreadcrumbBarWidget::clearSegments()
{
    while (m_segmentsLayout->count() > 1) { // Keep the stretch
        QLayoutItem *item = m_segmentsLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void BreadcrumbBarWidget::addSegment(const QString &text, const QString &tooltip)
{
    QLabel *separator = new QLabel(" > ", this);
    separator->setStyleSheet("color: palette(mid); font-size: 11px;");
    m_segmentsLayout->insertWidget(m_segmentsLayout->count() - 1, separator);

    QLabel *segment = new QLabel(text, this);
    segment->setStyleSheet("color: palette(text); font-size: 11px; padding: 0 2px;");
    if (!tooltip.isEmpty()) {
        segment->setToolTip(tooltip);
    }
    segment->setCursor(Qt::PointingHandCursor);
    m_segmentsLayout->insertWidget(m_segmentsLayout->count() - 1, segment);
}

void BreadcrumbBarWidget::updateForEditor(CodeEditor *editor, CssBreadcrumbParser *parser)
{
    clearSegments();

    if (!editor) return;

    QString filePath = editor->filePath();
    if (filePath.isEmpty()) {
        addSegment(tr("(untitled)"));
        return;
    }

    // Add file path segments
    QFileInfo fi(filePath);
    QStringList pathParts;
    pathParts.prepend(fi.fileName());
    QDir dir = fi.absoluteDir();
    while (dir.exists() && dir.path() != QDir::rootPath() && dir.path() != ".") {
        pathParts.prepend(dir.dirName());
        dir.cdUp();
    }

    for (const QString &part : pathParts) {
        addSegment(part, QString());
    }

    // For HTML/CSS files, add DOM hierarchy
    if (parser && editor) {
        QTextCursor cursor = editor->textCursor();
        QList<DomBreadcrumbElement> hierarchy = parser->parseDomHierarchy(
            editor, cursor.blockNumber(), cursor.positionInBlock());

        if (!hierarchy.isEmpty()) {
            for (const DomBreadcrumbElement &elem : hierarchy) {
                QString text = elem.tag;
                if (!elem.id.isEmpty()) text += "#" + elem.id;
                if (!elem.classes.isEmpty()) {
                    QString classes = elem.classes;
                    text += "." + classes.replace(' ', '.');
                }
                addSegment(text, QString());
            }
        }
    }
}

void BreadcrumbBarWidget::clear()
{
    clearSegments();
}
