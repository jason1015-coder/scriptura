#include "pluginmanagerdialog.h"
#include "rust_adapter.h"
#include <QJsonArray>
#include <QBrush>
#include <QColor>
#include <QButtonGroup>

PluginManagerDialog::PluginManagerDialog(RustPluginManagerAdapter *manager, PluginContext *context, QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
    , m_context(context)
    , m_pluginList(new QListWidget(this))
    , m_detailsText(new QTextEdit(this))
    , m_installButton(new QPushButton(tr("Install Plugin..."), this))
    , m_removeButton(new QPushButton(tr("Remove Plugin"), this))
    , m_toggleStateButton(new QPushButton(tr("Disable"), this))
    , m_refreshButton(new QPushButton(tr("Refresh"), this))
    , m_closeButton(new QPushButton(tr("Close"), this))
{
    setWindowTitle(tr("Plugin Manager"));
    setMinimumSize(700, 450);

    m_detailsText->setReadOnly(true);
    m_detailsText->setPlaceholderText(tr("Select a plugin to view details."));
    m_removeButton->setEnabled(false);
    m_toggleStateButton->setEnabled(false);

    auto *mainLayout = new QHBoxLayout(this);
    auto *leftLayout = new QVBoxLayout();
    auto *rightLayout = new QVBoxLayout();
    auto *buttonLayout = new QHBoxLayout();

    leftLayout->addWidget(new QLabel(tr("Installed Plugins:")));
    leftLayout->addWidget(m_pluginList);

    buttonLayout->addWidget(m_installButton);
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addWidget(m_toggleStateButton);
    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_closeButton);

    rightLayout->addWidget(new QLabel(tr("Plugin Details:")));
    rightLayout->addWidget(m_detailsText);
    rightLayout->addLayout(buttonLayout);

    mainLayout->addLayout(leftLayout, 1);
    mainLayout->addLayout(rightLayout, 2);

    connect(m_installButton, &QPushButton::clicked, this, &PluginManagerDialog::installPlugin);
    connect(m_removeButton, &QPushButton::clicked, this, &PluginManagerDialog::removeSelectedPlugin);
    connect(m_toggleStateButton, &QPushButton::clicked, this, &PluginManagerDialog::togglePluginState);
    connect(m_refreshButton, &QPushButton::clicked, this, &PluginManagerDialog::refreshPluginList);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_pluginList, &QListWidget::itemSelectionChanged, this, &PluginManagerDialog::onPluginSelectionChanged);
    connect(m_pluginList, &QListWidget::itemDoubleClicked, this, &PluginManagerDialog::onPluginDoubleClicked);

    refreshPluginList();
}

PluginManagerDialog::~PluginManagerDialog()
{
}

void PluginManagerDialog::refresh()
{
    refreshPluginList();
}

void PluginManagerDialog::refreshPluginList()
{
    m_pluginList->clear();
    m_plugins.clear();
    clearDetails();

    loadAvailablePlugins();

    for (const auto &plugin : std::as_const(m_plugins)) {
        auto *item = new QListWidgetItem(plugin.name);
        item->setData(Qt::UserRole, plugin.path);
        QString status;
        if (!plugin.enabled) {
            status = tr(" (Disabled)");
            item->setForeground(QBrush(QColor("#9E9E9E")));
        } else if (plugin.loaded) {
            status = tr(" (Loaded)");
            item->setForeground(QBrush(QColor("#4CAF50")));
        } else {
            status = tr(" (Not Loaded)");
            item->setForeground(QBrush(QColor("#FF9800")));
        }
        item->setText(plugin.name + status);
        m_pluginList->addItem(item);
    }
}

void PluginManagerDialog::loadAvailablePlugins()
{
    QString pluginDir = getPluginInstallDir();
    QDir dir(pluginDir);
    if (!dir.exists()) {
        QDir().mkpath(pluginDir);
    }

    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::Name);
    QList<QFileInfo> subdirs = dir.entryInfoList();

    QSet<QString> loadedIds;
    if (m_manager) {
        QStringList loaded = m_manager->listLoaded();
        for (const QString &id : loaded) {
            loadedIds.insert(id);
        }
    }

    for (const auto &info : subdirs) {
        if (!info.isDir()) continue;

        QString metadataPath = info.absoluteFilePath() + "/plugin.json";
        if (!QFile::exists(metadataPath)) continue;

        PluginInfo pInfo = getPluginInfo(info.absoluteFilePath(), false);
        if (pInfo.id.isEmpty()) continue;

        pInfo.loaded = loadedIds.contains(pInfo.id);
        pInfo.enabled = true; // Rust adapter doesn't track enabled/disabled separately
        m_plugins.append(pInfo);
    }
}

bool PluginManagerDialog::loadPluginMetadata(const QString &dirPath, QJsonObject &metadata)
{
    QString metadataFile = dirPath + "/plugin.json";
    QFile file(metadataFile);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray content = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(content, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    metadata = doc.object();
    return true;
}

PluginManagerDialog::PluginInfo PluginManagerDialog::getPluginInfo(const QString &dirPath, bool loaded)
{
    PluginInfo info;
    info.path = dirPath;
    info.loaded = loaded;

    QJsonObject metadata;
    if (!loadPluginMetadata(dirPath, metadata)) {
        return info;
    }

    info.id = metadata.value("id").toString();
    info.name = metadata.value("name").toString();
    info.version = metadata.value("version").toString();
    info.author = metadata.value("author").toString();
    info.description = metadata.value("description").toString();
    info.library = metadata.value("library").toString();

    if (metadata.contains("dependencies") && metadata["dependencies"].isArray()) {
        QJsonArray deps = metadata["dependencies"].toArray();
        for (const auto &dep : deps) {
            info.dependencies.append(dep.toString());
        }
    }

    if (metadata.contains("permissions") && metadata["permissions"].isArray()) {
        QJsonArray perms = metadata["permissions"].toArray();
        for (const auto &perm : perms) {
            info.permissions.append(perm.toString());
        }
    }

    if (info.name.isEmpty()) {
        info.name = QDir(dirPath).dirName();
    }

    return info;
}

QString PluginManagerDialog::getPluginInstallDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugins";
}

void PluginManagerDialog::installPlugin()
{
    QString sourceDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Plugin Directory"),
        QDir::homePath()
    );

    if (sourceDir.isEmpty()) {
        return;
    }

    QString metadataPath = sourceDir + "/plugin.json";
    if (!QFile::exists(metadataPath)) {
        QMessageBox::warning(this, tr("Invalid Plugin"), tr("The selected directory does not contain a plugin.json file."));
        return;
    }

    QJsonObject metadata;
    if (!loadPluginMetadata(sourceDir, metadata)) {
        QMessageBox::warning(this, tr("Invalid Plugin"), tr("Could not parse plugin.json."));
        return;
    }

    QString pluginId = metadata.value("id").toString();
    if (pluginId.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Plugin"), tr("Plugin metadata is missing an 'id' field."));
        return;
    }

    QString targetDir = getPluginInstallDir() + "/" + pluginId;
    QDir().mkpath(getPluginInstallDir());

    if (QDir(targetDir).exists()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("Plugin Already Exists"),
            tr("A plugin with ID '%1' is already installed. Do you want to replace it?").arg(pluginId)
        );
        if (reply != QMessageBox::Yes) {
            return;
        }
        removePluginDir(targetDir);
    }

    if (!installPluginFromDir(sourceDir, targetDir)) {
        QMessageBox::critical(this, tr("Installation Failed"), tr("Failed to copy plugin files to the installation directory."));
        return;
    }

    if (m_manager) {
        m_manager->loadPlugin(targetDir);
    }

    QMessageBox::information(this, tr("Plugin Installed"), tr("Plugin '%1' has been installed successfully. Restart Scriptura to load it.").arg(pluginId));
    refreshPluginList();
}

bool PluginManagerDialog::installPluginFromDir(const QString &sourceDir, const QString &targetDir)
{
    QDir source(sourceDir);
    QDir target(targetDir);

    if (!target.mkpath(".")) {
        return false;
    }

    QFileInfoList entries = source.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &info : entries) {
        QString destPath = target.absoluteFilePath(info.fileName());
        if (info.isDir()) {
            if (!installPluginFromDir(info.absoluteFilePath(), destPath)) {
                return false;
            }
        } else {
            if (!QFile::copy(info.absoluteFilePath(), destPath)) {
                return false;
            }
        }
    }

    return true;
}

void PluginManagerDialog::removeSelectedPlugin()
{
    int currentRow = m_pluginList->currentRow();
    if (currentRow < 0 || currentRow >= m_plugins.size()) {
        return;
    }

    const PluginInfo &plugin = m_plugins[currentRow];

    if (plugin.loaded) {
        QMessageBox::information(
            this,
            tr("Plugin In Use"),
            tr("Plugin '%1' is currently loaded. Restart Scriptura without this plugin to remove it, or disable it first.").arg(plugin.name)
        );
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Remove Plugin"),
        tr("Are you sure you want to remove plugin '%1'?").arg(plugin.name)
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    if (removePluginDir(plugin.path)) {
        QMessageBox::information(this, tr("Plugin Removed"), tr("Plugin '%1' has been removed.").arg(plugin.name));
        refreshPluginList();
    } else {
        QMessageBox::critical(this, tr("Removal Failed"), tr("Failed to remove plugin directory: %1").arg(plugin.path));
    }
}

bool PluginManagerDialog::removePluginDir(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        return true;
    }

    QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &info : entries) {
        if (info.isDir()) {
            if (!removePluginDir(info.absoluteFilePath())) {
                return false;
            }
        } else {
            if (!QFile::remove(info.absoluteFilePath())) {
                return false;
            }
        }
    }

    return dir.rmdir(dirPath);
}

void PluginManagerDialog::onPluginSelectionChanged()
{
    int currentRow = m_pluginList->currentRow();
    if (currentRow >= 0 && currentRow < m_plugins.size()) {
        m_removeButton->setEnabled(!m_plugins[currentRow].loaded);
        m_toggleStateButton->setEnabled(true);
        if (m_plugins[currentRow].enabled) {
            m_toggleStateButton->setText(tr("Disable"));
        } else {
            m_toggleStateButton->setText(tr("Enable"));
        }
        showPluginDetails(m_plugins[currentRow]);
    } else {
        m_removeButton->setEnabled(false);
        m_toggleStateButton->setEnabled(false);
        clearDetails();
    }
}

void PluginManagerDialog::onPluginDoubleClicked(QListWidgetItem *item)
{
    if (item) {
        int row = m_pluginList->row(item);
        if (row >= 0 && row < m_plugins.size()) {
            showPluginDetails(m_plugins[row]);
        }
    }
}

void PluginManagerDialog::showPluginDetails(const PluginInfo &info)
{
    QString details;
    details += tr("<b>ID:</b> %1<br/>").arg(info.id);
    details += tr("<b>Name:</b> %1<br/>").arg(info.name);
    details += tr("<b>Version:</b> %1<br/>").arg(info.version);
    details += tr("<b>Author:</b> %1<br/>").arg(info.author);
    details += tr("<b>Library:</b> %1<br/>").arg(info.library);
    details += tr("<b>Status:</b> %1<br/>").arg(info.loaded ? tr("Loaded") : tr("Not Loaded"));
    details += tr("<b>Enabled:</b> %1<br/>").arg(info.enabled ? tr("Yes") : tr("No"));
    details += tr("<b>Path:</b> %1<br/>").arg(info.path);

    if (!info.description.isEmpty()) {
        details += tr("<br/><b>Description:</b><br/>%1").arg(info.description.toHtmlEscaped());
    }

    if (!info.dependencies.isEmpty()) {
        details += tr("<br/><b>Dependencies:</b><br/>");
        for (const QString &dep : info.dependencies) {
            details += "  - " + dep.toHtmlEscaped() + "<br/>";
        }
    }

    if (!info.permissions.isEmpty()) {
        details += tr("<b>Permissions:</b><br/>");
        for (const QString &perm : info.permissions) {
            details += "  - " + perm.toHtmlEscaped() + "<br/>";
        }
    }

    m_detailsText->setHtml(details);
}

void PluginManagerDialog::clearDetails()
{
    m_detailsText->clear();
    m_detailsText->setPlaceholderText(tr("Select a plugin to view details."));
}

void PluginManagerDialog::togglePluginState()
{
    int currentRow = m_pluginList->currentRow();
    if (currentRow < 0 || currentRow >= m_plugins.size()) {
        return;
    }

    PluginInfo &plugin = m_plugins[currentRow];
    if (!m_manager) {
        return;
    }

    if (plugin.enabled) {
        m_manager->unloadPlugin(plugin.id);
        plugin.enabled = false;
        m_toggleStateButton->setText(tr("Enable"));
        QMessageBox::information(this, tr("Plugin Disabled"), tr("Plugin '%1' has been disabled.").arg(plugin.name));
    } else {
        m_manager->loadPlugin(plugin.path);
        plugin.enabled = true;
        m_toggleStateButton->setText(tr("Disable"));
        QMessageBox::information(this, tr("Plugin Enabled"), tr("Plugin '%1' has been enabled.").arg(plugin.name));
    }

    refreshPluginList();
}
