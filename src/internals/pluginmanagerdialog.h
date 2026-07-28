#ifndef PLUGINMANAGERDIALOG_H
#define PLUGINMANAGERDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QStandardPaths>
#include "plugincontext.h"

class RustPluginManagerAdapter;

class PluginManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PluginManagerDialog(RustPluginManagerAdapter *manager, PluginContext *context, QWidget *parent = nullptr);
    ~PluginManagerDialog() override;

    void refresh();

private slots:
    void refreshPluginList();
    void installPlugin();
    void removeSelectedPlugin();
    void togglePluginState();
    void onPluginSelectionChanged();
    void onPluginDoubleClicked(QListWidgetItem *item);

private:
    struct PluginInfo {
        QString path;
        QString id;
        QString name;
        QString version;
        QString author;
        QString description;
        QString library;
        QStringList dependencies;
        QStringList permissions;
        bool loaded;
        bool enabled;
    };

    void loadAvailablePlugins();
    bool loadPluginMetadata(const QString &dirPath, QJsonObject &metadata);
    PluginInfo getPluginInfo(const QString &dirPath, bool loaded);
    bool installPluginFromDir(const QString &sourceDir, const QString &targetDir);
    bool removePluginDir(const QString &dirPath);
    void showPluginDetails(const PluginInfo &info);
    void clearDetails();

public:
    QString getPluginInstallDir() const;

    RustPluginManagerAdapter *m_manager;
    PluginContext *m_context;
    QListWidget *m_pluginList;
    QTextEdit *m_detailsText;
    QPushButton *m_installButton;
    QPushButton *m_removeButton;
    QPushButton *m_toggleStateButton;
    QPushButton *m_refreshButton;
    QPushButton *m_closeButton;
    QList<PluginInfo> m_plugins;
};

#endif // PLUGINMANAGERDIALOG_H
