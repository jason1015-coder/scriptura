#include "pluginmarketplace.h"
#include "rust_adapter.h"
#include <QJsonDocument>

PluginMarketplaceWidget::PluginMarketplaceWidget(RustPluginRegistryAdapter *registry, QWidget *parent)
    : QWidget(parent)
    , m_registry(registry)
{
    setupUI();

    if (m_registry) {
        connect(m_registry, &RustPluginRegistryAdapter::registryUpdated, this, [this](const QString &manifestJson) {
            QJsonDocument doc = QJsonDocument::fromJson(manifestJson.toUtf8());
            m_allPlugins = doc.object()["plugins"].toArray();
            populateTree(m_allPlugins);
            m_statusLabel->setText(tr("%1 plugins available").arg(m_allPlugins.size()));
        });
    }
}

void PluginMarketplaceWidget::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    QLabel *title = new QLabel(tr("Plugin Marketplace"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    title->setFont(titleFont);
    layout->addWidget(title);

    QHBoxLayout *searchLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search plugins..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &PluginMarketplaceWidget::onSearchChanged);
    searchLayout->addWidget(m_searchEdit, 1);
    layout->addLayout(searchLayout);

    m_pluginTree = new QTreeWidget(this);
    m_pluginTree->setHeaderLabels({tr("Name"), tr("Version"), tr("Author"), tr("Status")});
    m_pluginTree->setColumnWidth(0, 180);
    m_pluginTree->setColumnWidth(1, 60);
    m_pluginTree->setColumnWidth(2, 120);
    m_pluginTree->setColumnWidth(3, 80);
    m_pluginTree->setAlternatingRowColors(true);
    m_pluginTree->setRootIsDecorated(false);
    connect(m_pluginTree, &QTreeWidget::itemSelectionChanged, this, &PluginMarketplaceWidget::onItemSelectionChanged);
    layout->addWidget(m_pluginTree, 1);

    m_detailLabel = new QLabel(tr("Select a plugin to see details"), this);
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setMinimumHeight(60);
    m_detailLabel->setStyleSheet("QLabel { padding: 8px; border: 1px solid palette(mid); border-radius: 6px; }");
    layout->addWidget(m_detailLabel);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_installBtn = new QPushButton(tr("Install"), this);
    m_installBtn->setToolTip(tr("Plugin installation will be available in a future release"));
    m_uninstallBtn = new QPushButton(tr("Uninstall"), this);
    m_uninstallBtn->setToolTip(tr("Plugin uninstallation will be available in a future release"));
    m_refreshBtn = new QPushButton(tr("Refresh"), this);
    m_installBtn->setEnabled(false);
    m_uninstallBtn->setEnabled(false);
    connect(m_installBtn, &QPushButton::clicked, this, &PluginMarketplaceWidget::onInstallClicked);
    connect(m_uninstallBtn, &QPushButton::clicked, this, &PluginMarketplaceWidget::onUninstallClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &PluginMarketplaceWidget::onRefreshClicked);
    btnLayout->addWidget(m_installBtn);
    btnLayout->addWidget(m_uninstallBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_refreshBtn);
    layout->addLayout(btnLayout);

    m_statusLabel = new QLabel(tr("Click Refresh to load plugins"), this);
    layout->addWidget(m_statusLabel);
}

void PluginMarketplaceWidget::refreshPlugins()
{
    if (m_registry) {
        m_registry->checkForUpdates();
        m_statusLabel->setText(tr("Refreshing..."));
    }
}

void PluginMarketplaceWidget::searchPlugins(const QString &query)
{
    if (query.isEmpty()) {
        populateTree(m_allPlugins);
        return;
    }

    QJsonArray filtered;
    for (const QJsonValue &v : m_allPlugins) {
        QJsonObject plugin = v.toObject();
        QString name = plugin["name"].toString().toLower();
        QString desc = plugin["description"].toString().toLower();
        QString author = plugin["author"].toString().toLower();
        QString queryLower = query.toLower();
        if (name.contains(queryLower) || desc.contains(queryLower) || author.contains(queryLower)) {
            filtered.append(plugin);
        }
    }
    populateTree(filtered);
}

void PluginMarketplaceWidget::populateTree(const QJsonArray &plugins)
{
    m_pluginTree->clear();

    for (const QJsonValue &v : plugins) {
        QJsonObject plugin = v.toObject();
        QTreeWidgetItem *item = new QTreeWidgetItem(m_pluginTree);
        item->setText(0, plugin["name"].toString());
        item->setText(1, plugin["version"].toString());
        item->setText(2, plugin["author"].toString());
        item->setText(3, plugin["installed"].toBool() ? tr("Installed") : tr("Available"));
        item->setData(0, Qt::UserRole, QJsonDocument(plugin).toJson());
        if (plugin["installed"].toBool()) {
            item->setForeground(3, QColor(0, 150, 0));
        }
    }
}

void PluginMarketplaceWidget::onSearchChanged(const QString &text)
{
    searchPlugins(text);
}

void PluginMarketplaceWidget::onInstallClicked()
{
    QTreeWidgetItem *item = m_pluginTree->currentItem();
    if (!item) return;

    QJsonObject plugin = QJsonDocument::fromJson(item->data(0, Qt::UserRole).toByteArray()).object();
    QString pluginName = plugin["name"].toString();
    m_statusLabel->setText(tr("%1 \u2014 installation coming soon").arg(pluginName));
}

void PluginMarketplaceWidget::onUninstallClicked()
{
    QTreeWidgetItem *item = m_pluginTree->currentItem();
    if (!item) return;

    QJsonObject plugin = QJsonDocument::fromJson(item->data(0, Qt::UserRole).toByteArray()).object();
    QString pluginName = plugin["name"].toString();
    m_statusLabel->setText(tr("%1 \u2014 uninstallation coming soon").arg(pluginName));
}

void PluginMarketplaceWidget::onRefreshClicked()
{
    refreshPlugins();
}

void PluginMarketplaceWidget::onItemSelectionChanged()
{
    QTreeWidgetItem *item = m_pluginTree->currentItem();
    if (!item) {
        m_detailLabel->setText(tr("Select a plugin to see details"));
        m_installBtn->setEnabled(false);
        m_uninstallBtn->setEnabled(false);
        return;
    }

    QJsonObject plugin = QJsonDocument::fromJson(item->data(0, Qt::UserRole).toByteArray()).object();
    QString detail = QString("<b>%1</b> v%2<br>%3<br><i>%4</i>")
        .arg(plugin["name"].toString(),
             plugin["version"].toString(),
             plugin["description"].toString(),
             plugin["author"].toString());
    m_detailLabel->setText(detail);

    bool installed = plugin["installed"].toBool();
    m_installBtn->setEnabled(!installed);
    m_uninstallBtn->setEnabled(installed);
}

void PluginMarketplaceWidget::updateButtonStates()
{
    onItemSelectionChanged();
}
