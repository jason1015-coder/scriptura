#ifndef PLUGINMARKETPLACE_H
#define PLUGINMARKETPLACE_H

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QUrl>

class RustPluginRegistryAdapter;

/**
 * Plugin Marketplace widget for searching, browsing, and installing plugins.
 * Features:
 * - Search plugins by name, category, or keyword
 * - Show plugin details (description, author, version)
 * - Install/uninstall/update plugins
 * - Show installed plugins with update notifications
 */
class PluginMarketplaceWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PluginMarketplaceWidget(RustPluginRegistryAdapter *registry, QWidget *parent = nullptr);

    void refreshPlugins();
    void searchPlugins(const QString &query);

signals:
    void pluginInstalled(const QString &pluginId);
    void pluginUninstalled(const QString &pluginId);

private slots:
    void onSearchChanged(const QString &text);
    void onInstallClicked();
    void onUninstallClicked();
    void onRefreshClicked();
    void onItemSelectionChanged();

private:
    void setupUI();
    void populateTree(const QJsonArray &plugins);
    void updateButtonStates();
    QUrl resolveDownloadUrl(const QString &downloadUrl) const;
    bool copyDirectory(const QString &srcPath, const QString &dstPath);

    QLineEdit *m_searchEdit;
    QTreeWidget *m_pluginTree;
    QLabel *m_detailLabel;
    QLabel *m_statusLabel;
    QPushButton *m_installBtn;
    QPushButton *m_uninstallBtn;
    QPushButton *m_refreshBtn;
    RustPluginRegistryAdapter *m_registry;
    QJsonArray m_allPlugins;
    QNetworkAccessManager *m_network;
};

#endif // PLUGINMARKETPLACE_H
