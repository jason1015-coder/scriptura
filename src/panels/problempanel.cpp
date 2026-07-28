#include "problempanel.h"
#include <QHeaderView>
#include <QApplication>
#include <QStyle>
#include <QFileInfo>
#include <QJsonObject>

ProblemPanel::ProblemPanel(QWidget *parent)
    : QWidget(parent)
    , m_treeWidget(new QTreeWidget(this))
    , m_filterTabs(new QTabBar(this))
    , m_closeButton(new QPushButton(this))
    , m_countLabel(new QLabel(this))
    , m_mainLayout(new QVBoxLayout(this))
    , m_currentFilter(All)
{
    setObjectName("problemPanel");

    QWidget *headerWidget = new QWidget(this);
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(4, 4, 4, 4);
    headerLayout->setSpacing(4);

    m_filterTabs->addTab(tr("All"));
    m_filterTabs->addTab(tr("Errors"));
    m_filterTabs->addTab(tr("Warnings"));
    m_filterTabs->addTab(tr("Info"));
    m_filterTabs->setCurrentIndex(0);
    m_filterTabs->setDrawBase(false);

    m_countLabel->setText("0 problems");
    m_countLabel->setStyleSheet("color: palette(mid);");

    m_closeButton->setText("×");
    m_closeButton->setFixedSize(24, 24);
    m_closeButton->setToolTip(tr("Close"));
    m_closeButton->setStyleSheet(
        "QPushButton { border: none; background: transparent; font-size: 16px; }"
        "QPushButton:hover { background: palette(highlight); color: white; border-radius: 8px; }"
    );

    headerLayout->addWidget(m_filterTabs);
    headerLayout->addWidget(m_countLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_closeButton);

    m_treeWidget->setHeaderLabels({tr("Problem"), tr("Location"), tr("Source")});
    m_treeWidget->setColumnWidth(0, 300);
    m_treeWidget->setColumnWidth(1, 100);
    m_treeWidget->setColumnWidth(2, 100);
    m_treeWidget->setRootIsDecorated(false);
    m_treeWidget->setItemsExpandable(false);
    m_treeWidget->setUniformRowHeights(true);
    m_treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    QString treeStyle = R"(
        QTreeWidget {
            background: palette(base);
            alternate-background-color: palette(alternate-base);
            border: none;
            outline: none;
        }
        QTreeWidget::item {
            padding: 4px;
            border-bottom: 1px solid palette(midlight);
        }
        QTreeWidget::item:selected {
            background: palette(highlight);
            color: palette(highlighted-text);
        }
        QTreeWidget::item:hover {
            background: palette(alternate-base);
        }
        QHeaderView::section {
            background: palette(button);
            padding: 4px;
            border: none;
            border-bottom: 1px solid palette(mid);
            font-weight: bold;
        }
    )";
    m_treeWidget->setStyleSheet(treeStyle);

    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    m_mainLayout->addWidget(headerWidget);
    m_mainLayout->addWidget(m_treeWidget);

    connect(m_filterTabs, &QTabBar::currentChanged, this, &ProblemPanel::onFilterTabChanged);
    connect(m_closeButton, &QPushButton::clicked, this, &ProblemPanel::onCloseClicked);
    connect(m_treeWidget, &QTreeWidget::itemActivated, this, &ProblemPanel::onItemActivated);
}

ProblemPanel::~ProblemPanel() = default;

void ProblemPanel::setProblems(const QString &fileUri, const QJsonArray &diagnostics)
{
    m_allProblems[fileUri] = diagnostics;
    if (fileUri == m_currentFile || m_currentFile.isEmpty()) {
        rebuildTree();
    }
}

void ProblemPanel::clearProblems(const QString &fileUri)
{
    m_allProblems.remove(fileUri);
    if (fileUri == m_currentFile) {
        rebuildTree();
    }
}

void ProblemPanel::clearAll()
{
    m_allProblems.clear();
    rebuildTree();
}

int ProblemPanel::problemCount(Filter filter) const
{
    if (filter == All)
        return m_filteredProblems.size();

    int count = 0;
    for (const ProblemItem &item : m_filteredProblems) {
        if (item.severity == filter)
            count++;
    }
    return count;
}

void ProblemPanel::setCurrentFile(const QString &fileUri)
{
    m_currentFile = fileUri;
    rebuildTree();
}

void ProblemPanel::setFilter(Filter filter)
{
    m_currentFilter = filter;
    m_filterTabs->setCurrentIndex(static_cast<int>(filter));
    rebuildTree();
}

void ProblemPanel::onItemActivated(QTreeWidgetItem *item, int /*column*/)
{
    if (!item)
        return;

    QString fileUri = item->data(0, Qt::UserRole).toString();
    int line = item->data(0, Qt::UserRole + 1).toInt();
    int col = item->data(0, Qt::UserRole + 2).toInt();

    emit problemActivated(fileUri, line, col);
}

void ProblemPanel::onFilterTabChanged(int index)
{
    m_currentFilter = static_cast<Filter>(index);
    rebuildTree();
    emit filterChanged(m_currentFilter);
}

void ProblemPanel::onCloseClicked()
{
    hide();
}

void ProblemPanel::onProblemsChanged(const QString &uri, const QJsonArray &diags)
{
    setProblems(uri, diags);
}

void ProblemPanel::rebuildTree()
{
    m_treeWidget->clear();
    m_filteredProblems.clear();

    QList<ProblemItem> items;

    if (!m_currentFile.isEmpty()) {
        QJsonArray diags = m_allProblems.value(m_currentFile);
        for (const QJsonValue &v : diags) {
            QJsonObject diag = v.toObject();
            ProblemItem item;
            item.fileUri = m_currentFile;
            item.line = diag["range"].toObject()["start"].toObject()["line"].toInt();
            item.column = diag["range"].toObject()["start"].toObject()["character"].toInt();
            item.severity = diag["severity"].toInt();
            item.message = diag["message"].toString();
            item.source = diag["source"].toString();
            items.append(item);
        }
    } else {
        for (auto it = m_allProblems.constBegin(); it != m_allProblems.constEnd(); ++it) {
            for (const QJsonValue &v : it.value()) {
                QJsonObject diag = v.toObject();
                ProblemItem item;
                item.fileUri = it.key();
                item.line = diag["range"].toObject()["start"].toObject()["line"].toInt();
                item.column = diag["range"].toObject()["start"].toObject()["character"].toInt();
                item.severity = diag["severity"].toInt();
                item.message = diag["message"].toString();
                item.source = diag["source"].toString();
                items.append(item);
            }
        }
    }

    for (const ProblemItem &item : items) {
        if (m_currentFilter == All || item.severity == m_currentFilter) {
            m_filteredProblems.append(item);
        }
    }

    std::sort(m_filteredProblems.begin(), m_filteredProblems.end(), [](const ProblemItem &a, const ProblemItem &b) {
        if (a.severity != b.severity)
            return a.severity < b.severity;
        if (a.line != b.line)
            return a.line < b.line;
        return a.column < b.column;
    });

    for (const ProblemItem &item : m_filteredProblems) {
        m_treeWidget->addTopLevelItem(createTreeItem(item));
    }

    int errorCount = problemCount(Errors);
    int warningCount = problemCount(Warnings);
    int infoCount = problemCount(Info);

    QString countText;
    if (errorCount > 0)
        countText = tr("%1 errors, %2 warnings").arg(errorCount).arg(warningCount);
    else if (warningCount > 0)
        countText = tr("%1 warnings").arg(warningCount);
    else if (infoCount > 0)
        countText = tr("%1 info").arg(infoCount);
    else
        countText = tr("No problems");

    m_countLabel->setText(countText);

    if (errorCount > 0)
        m_countLabel->setStyleSheet("color: #ff5555; font-weight: bold;");
    else if (warningCount > 0)
        m_countLabel->setStyleSheet("color: #ffaa00; font-weight: bold;");
    else
        m_countLabel->setStyleSheet("color: palette(mid);");
}

void ProblemPanel::addProblemItem(const ProblemItem &item)
{
    m_filteredProblems.append(item);
    m_treeWidget->addTopLevelItem(createTreeItem(item));
}

QTreeWidgetItem *ProblemPanel::createTreeItem(const ProblemItem &item) const
{
    QTreeWidgetItem *treeItem = new QTreeWidgetItem();
    treeItem->setText(0, item.message);
    treeItem->setText(1, tr("Line %1, Col %2").arg(item.line + 1).arg(item.column + 1));
    treeItem->setText(2, item.source);

    treeItem->setIcon(0, QIcon::fromTheme(severityIcon(item.severity)));

    QColor color = severityColor(item.severity);
    treeItem->setForeground(0, color);
    treeItem->setForeground(1, color);

    treeItem->setData(0, Qt::UserRole, item.fileUri);
    treeItem->setData(0, Qt::UserRole + 1, item.line);
    treeItem->setData(0, Qt::UserRole + 2, item.column);

    return treeItem;
}

QString ProblemPanel::severityIcon(int severity) const
{
    switch (severity) {
    case 1: return "dialog-error";
    case 2: return "dialog-warning";
    case 3: return "dialog-information";
    case 4: return "help-hint";
    }
    return "dialog-information";
}

QString ProblemPanel::severityText(int severity) const
{
    switch (severity) {
    case 1: return tr("Error");
    case 2: return tr("Warning");
    case 3: return tr("Info");
    case 4: return tr("Hint");
    }
    return tr("Unknown");
}

QColor ProblemPanel::severityColor(int severity) const
{
    switch (severity) {
    case 1: return QColor(255, 80, 80);
    case 2: return QColor(255, 170, 50);
    case 3: return QColor(50, 150, 255);
    case 4: return QColor(150, 150, 150);
    }
    return QApplication::palette().color(QPalette::Text);
}
