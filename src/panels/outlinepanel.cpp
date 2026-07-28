#include "outlinepanel.h"
#include "rust_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>

OutlinePanel::OutlinePanel(QWidget *parent)
    : QWidget(parent)
    , m_lspClient(nullptr)
{
    setupUI();
    connect(m_filterEdit, &QLineEdit::textChanged, this, &OutlinePanel::onFilterChanged);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &OutlinePanel::onItemDoubleClicked);
    connect(m_refreshButton, &QPushButton::clicked, this, &OutlinePanel::onRefreshClicked);
}

void OutlinePanel::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Header
    QWidget *header = new QWidget(this);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 4, 8, 4);
    
    QLabel *titleLabel = new QLabel(tr("Outline"), this);
    titleLabel->setStyleSheet("font-weight: bold;");
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter symbols..."));
    m_filterEdit->setClearButtonEnabled(true);
    m_refreshButton = new QPushButton(tr("↻"), this);
    m_refreshButton->setToolTip(tr("Refresh"));
    m_refreshButton->setFixedSize(24, 24);
    
    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(m_filterEdit, 1);
    headerLayout->addWidget(m_refreshButton);
    
    mainLayout->addWidget(header);
    
    // Tree widget
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(16);
    m_tree->setAnimated(true);
    m_tree->setStyleSheet(R"(
        QTreeWidget {
            border: none;
            background: palette(base);
            font-size: 12px;
        }
        QTreeWidget::item {
            padding: 2px 4px;
        }
        QTreeWidget::item:selected {
            background: palette(highlight);
            color: palette(highlighted-text);
        }
    )");
    mainLayout->addWidget(m_tree, 1);
    
    // Status
    m_statusLabel = new QLabel(tr("No symbols"), this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("color: palette(mid); padding: 4px;");
    mainLayout->addWidget(m_statusLabel);
}

void OutlinePanel::setSymbols(const QList<LspSymbolTypes::SymbolInformation> &symbols)
{
    m_symbols = symbols;
    populateTree(symbols);
}

void OutlinePanel::clear()
{
    m_tree->clear();
    m_symbols.clear();
    m_statusLabel->setText(tr("No symbols"));
}

void OutlinePanel::refresh()
{
    if (!m_filePath.isEmpty() && m_lspClient && m_lspClient->isRunning()) {
        m_lspClient->documentSymbol(m_filePath);
    }
}

void OutlinePanel::setFilePath(const QString &path)
{
    m_filePath = path;
}

void OutlinePanel::setLspClient(RustLspClientAdapter *client)
{
    m_lspClient = client;
}

void OutlinePanel::onFilterChanged(const QString &text)
{
    if (text.isEmpty()) {
        // Show all items
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = m_tree->topLevelItem(i);
            item->setHidden(false);
            for (int j = 0; j < item->childCount(); ++j) {
                item->child(j)->setHidden(false);
            }
        }
    } else {
        // Filter items
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = m_tree->topLevelItem(i);
            bool match = item->text(0).contains(text, Qt::CaseInsensitive);
            item->setHidden(!match);
            
            // Also check children
            for (int j = 0; j < item->childCount(); ++j) {
                QTreeWidgetItem *child = item->child(j);
                bool childMatch = child->text(0).contains(text, Qt::CaseInsensitive);
                child->setHidden(!childMatch);
                if (childMatch) match = true;
            }
            item->setHidden(!match);
        }
    }
    
    emit filterChanged(text);
}

void OutlinePanel::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    
    if (!item) return;
    
    // Get line and column from item data
    int line = item->data(0, Qt::UserRole).toInt();
    int col = item->data(0, Qt::UserRole + 1).toInt();
    QString filePath = item->data(0, Qt::UserRole + 2).toString();
    
    if (filePath.isEmpty()) {
        filePath = m_filePath;
    }
    
    emit symbolActivated(filePath, line, col);
}

void OutlinePanel::onRefreshClicked()
{
    refresh();
}

void OutlinePanel::populateTree(const QList<LspSymbolTypes::SymbolInformation> &symbols)
{
    m_tree->clear();
    
    if (symbols.isEmpty()) {
        m_statusLabel->setText(tr("No symbols"));
        return;
    }
    
    // Build hierarchical structure
    QMap<QString, QTreeWidgetItem*> parentMap;
    
    for (const LspSymbolTypes::SymbolInformation &symbol : symbols) {
        QTreeWidgetItem *item = createItem(symbol);
        
        // Try to find parent container
        QString containerName = symbol.containerName;
        if (!containerName.isEmpty() && parentMap.contains(containerName)) {
            parentMap[containerName]->addChild(item);
        } else {
            m_tree->addTopLevelItem(item);
            parentMap[symbol.name] = item;
        }
    }
    
    m_statusLabel->setText(tr("%1 symbol(s)").arg(symbols.size()));
    m_tree->expandAll();
}

QTreeWidgetItem *OutlinePanel::createItem(const LspSymbolTypes::SymbolInformation &symbol)
{
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setText(0, symbol.name);
    item->setIcon(0, iconForKind(symbol.kind));
    item->setData(0, Qt::UserRole, symbol.range.start.line);
    item->setData(0, Qt::UserRole + 1, symbol.range.start.character);
    item->setData(0, Qt::UserRole + 2, symbol.uri);
    item->setToolTip(0, QString("%1 (%2)").arg(symbol.name, symbol.kind));
    
    return item;
}

QIcon OutlinePanel::iconForKind(const QString &kind) const
{
    // Return appropriate icon based on symbol kind
    // This is simplified - in production, use actual icons
    if (kind == "Function" || kind == "Method") {
        return QIcon(":/icons/method.svg");  // Would need to add these icons
    } else if (kind == "Class" || kind == "Struct") {
        return QIcon(":/icons/class.svg");
    } else if (kind == "Variable" || kind == "Field") {
        return QIcon(":/icons/variable.svg");
    } else if (kind == "Interface") {
        return QIcon(":/icons/interface.svg");
    } else if (kind == "Enum") {
        return QIcon(":/icons/enum.svg");
    } else if (kind == "Property") {
        return QIcon(":/icons/property.svg");
    } else if (kind == "Module" || kind == "Namespace") {
        return QIcon(":/icons/namespace.svg");
    } else if (kind == "String" || kind == "Number" || kind == "Boolean") {
        return QIcon(":/icons/literal.svg");
    }
    
    return QIcon(":/icons/symbol.svg");  // Default symbol icon
}
