#include "pluginmarketplace.h"
#include "rust_adapter.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryDir>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QUrl>

PluginMarketplaceWidget::PluginMarketplaceWidget(RustPluginRegistryAdapter *registry, QWidget *parent)
    : QWidget(parent)
    , m_registry(registry)
    , m_network(new QNetworkAccessManager(this))
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
    m_installBtn->setToolTip(tr("Download and install the selected plugin from the registry"));
    m_uninstallBtn = new QPushButton(tr("Uninstall"), this);
    m_uninstallBtn->setToolTip(tr("Remove the selected installed plugin"));
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
    QString pluginId = plugin["id"].toString();
    QString pluginName = plugin["name"].toString();
    QString downloadUrl = plugin["downloadUrl"].toString();

    if (downloadUrl.isEmpty()) {
        m_statusLabel->setText(tr("Cannot install %1: no download URL in registry").arg(pluginName));
        return;
    }

    m_installBtn->setEnabled(false);
    m_refreshBtn->setEnabled(false);
    m_statusLabel->setText(tr("Downloading %1...").arg(pluginName));

    // Resolve download URL (handle GitHub repos -> archive ZIP conversion)
    QUrl url = resolveDownloadUrl(downloadUrl);

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_network->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, pluginId, pluginName]() {
        reply->deleteLater();
        m_refreshBtn->setEnabled(true);

        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(tr("Download failed for %1: %2").arg(pluginName, reply->errorString()));
            m_installBtn->setEnabled(true);
            return;
        }

        QByteArray archiveData = reply->readAll();
        if (archiveData.isEmpty()) {
            m_statusLabel->setText(tr("Downloaded empty archive for %1").arg(pluginName));
            m_installBtn->setEnabled(true);
            return;
        }

        m_statusLabel->setText(tr("Extracting %1...").arg(pluginName));

        // Extract to a temporary directory using the Rust ArchiveExtractor
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            m_statusLabel->setText(tr("Failed to create temporary directory"));
            m_installBtn->setEnabled(true);
            return;
        }

        RustArchiveExtractorAdapter *extractor = RustBackend::instance()->archiveExtractor();
        if (!extractor || !extractor->extract(archiveData, tempDir.path())) {
            m_statusLabel->setText(tr("Failed to extract %1 archive — corrupted or unsupported format").arg(pluginName));
            m_installBtn->setEnabled(true);
            return;
        }

        // Find plugin.json in the extracted files (may be in a subdirectory)
        QString pluginSourceDir;
        QDirIterator it(tempDir.path(), QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            if (QFile::exists(it.filePath() + "/plugin.json")) {
                pluginSourceDir = it.filePath();
                break;
            }
        }
        // Also check the root
        if (pluginSourceDir.isEmpty() && QFile::exists(tempDir.path() + "/plugin.json")) {
            pluginSourceDir = tempDir.path();
        }

        if (pluginSourceDir.isEmpty()) {
            m_statusLabel->setText(tr("No plugin.json found in the downloaded archive for %1").arg(pluginName));
            m_installBtn->setEnabled(true);
            return;
        }

        m_statusLabel->setText(tr("Installing %1...").arg(pluginName));

        // Determine install directory
        QString installDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                             + "/plugins/" + pluginId;
        QDir().mkpath(installDir);

        // Remove any previous installation
        if (QDir(installDir).exists()) {
            QDir oldDir(installDir);
            oldDir.removeRecursively();
        }

        // Copy the plugin files
        if (!copyDirectory(pluginSourceDir, installDir)) {
            m_statusLabel->setText(tr("Failed to copy plugin files for %1").arg(pluginName));
            m_installBtn->setEnabled(true);
            return;
        }

        // Ensure shared libraries are executable (QFile::copy does not preserve permissions)
        QDirIterator libIt(installDir, {"*.so", "*.dylib", "*.dll"}, QDir::Files, QDirIterator::Subdirectories);
        while (libIt.hasNext()) {
            QFile::setPermissions(libIt.next(),
                QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                QFile::ReadGroup | QFile::ExeGroup |
                QFile::ReadOther | QFile::ExeOther);
        }

        // Load the plugin into the Rust plugin manager
        RustPluginManagerAdapter *pm = RustBackend::instance()->pluginManager();
        if (pm) {
            pm->loadPlugin(installDir);
        }

        m_statusLabel->setText(tr("%1 installed successfully! Restart Scriptura to activate.").arg(pluginName));
        m_installBtn->setEnabled(true);

        // Refresh the registry to update the installed status
        refreshPlugins();

        emit pluginInstalled(pluginId);
    });
}

void PluginMarketplaceWidget::onUninstallClicked()
{
    QTreeWidgetItem *item = m_pluginTree->currentItem();
    if (!item) return;

    QJsonObject plugin = QJsonDocument::fromJson(item->data(0, Qt::UserRole).toByteArray()).object();
    QString pluginId = plugin["id"].toString();
    QString pluginName = plugin["name"].toString();

    QString installDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                         + "/plugins/" + pluginId;

    if (!QDir(installDir).exists()) {
        m_statusLabel->setText(tr("%1 is not installed").arg(pluginName));
        return;
    }

    // Confirm with user
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Uninstall Plugin"),
        tr("Are you sure you want to uninstall '%1'?\n\nThis will remove the plugin from:\n%2")
            .arg(pluginName, installDir));

    if (reply != QMessageBox::Yes) return;

    m_uninstallBtn->setEnabled(false);
    m_statusLabel->setText(tr("Uninstalling %1...").arg(pluginName));

    // Unload from the plugin manager
    RustPluginManagerAdapter *pm = RustBackend::instance()->pluginManager();
    if (pm && pm->isLoaded(pluginId)) {
        pm->unloadPlugin(pluginId);
    }

    // Remove the plugin directory
    QDir dir(installDir);
    if (dir.removeRecursively()) {
        m_statusLabel->setText(tr("%1 uninstalled successfully. Restart Scriptura to complete.").arg(pluginName));

        // Refresh the registry to update the installed status
        refreshPlugins();

        emit pluginUninstalled(pluginId);
    } else {
        m_statusLabel->setText(tr("Failed to remove %1 — the directory may be in use").arg(pluginName));
    }

    m_uninstallBtn->setEnabled(true);
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

/// Resolve a plugin download URL to a downloadable archive URL.
/// - GitHub repo URLs are converted to archive ZIP URLs.
/// - Direct .zip URLs are used as-is.
QUrl PluginMarketplaceWidget::resolveDownloadUrl(const QString &downloadUrl) const
{
    // GitHub repo URL -> archive download
    // e.g. https://github.com/scriptura/git-app -> https://github.com/scriptura/git-app/archive/refs/heads/main.zip
    static const QString githubPrefix = "https://github.com/";
    if (downloadUrl.startsWith(githubPrefix)) {
        QString base = downloadUrl;
        if (base.endsWith('/')) base.chop(1);
        if (base.endsWith(".git")) base.chop(4);
        return QUrl(base + "/archive/refs/heads/main.zip");
    }

    // Direct ZIP URL
    if (downloadUrl.endsWith(".zip", Qt::CaseInsensitive)) {
        return QUrl(downloadUrl);
    }

    // Fallback: append /archive/master.zip for generic git hosting
    QString base = downloadUrl;
    if (base.endsWith('/')) base.chop(1);
    return QUrl(base + "/archive/master.zip");
}

/// Recursively copy a directory tree from srcPath to dstPath.
bool PluginMarketplaceWidget::copyDirectory(const QString &srcPath, const QString &dstPath)
{
    QDir srcDir(srcPath);
    QDir dstDir(dstPath);

    if (!dstDir.mkpath("."))
        return false;

    QFileInfoList entries = srcDir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &info : entries) {
        QString dest = dstDir.absoluteFilePath(info.fileName());
        if (info.isDir()) {
            if (!copyDirectory(info.absoluteFilePath(), dest))
                return false;
        } else {
            if (!QFile::copy(info.absoluteFilePath(), dest))
                return false;
        }
    }
    return true;
}
