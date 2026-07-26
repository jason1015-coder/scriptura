#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <QObject>
#include <QPluginLoader>
#include <QJsonObject>
#include <QHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVariant>
#include <QMutex>
#include <QMutexLocker>
#include <QSharedPointer>
#include <memory>
#include <functional>
#include "plugininterface.h"
#include "permission.h"

struct CrashInfo;
class RustBackend;

/**
 * @file pluginmanager.h
 * @brief 定義插件管理器，負責插件的載入、卸載和生命週期管理
 * 
 * PluginManager 是插件系統的核心管理類別，負責：
 * - 插件發現和掃描
 * - 依賴解析和載入順序計算
 * - 插件生命週期管理
 * - 事件系統協調
 * - 權限管理
 * - 沙箱外掛程序隔離
 */

/**
 * @class PluginManager
 * @brief 管理所有插件的載入、卸載和生命週期
 * 
 * 這個類別實作了插件系統的核心邏輯，包括：
 * - 掃描插件目錄發現可用插件
 * - 解析插件元數據
 * - 處理插件依賴關係
 * - 管理插件載入順序
 * - 處理插件事件
 * - 強制執行插件權限
 * - 外掛程序隔離（sandbox 模式）
 */
class PluginManager : public QObject
{
    Q_OBJECT

public:
     /**
      * @brief 建構函數
      * @param parent 父物件
      */
     explicit PluginManager(QObject* parent = nullptr);
     
     /**
      * @brief 設定插件上下文
      * @param context 插件上下文指標
      */
     void setContext(PluginContext* context);
     
     /**
      * @brief 解構函數
      */
     ~PluginManager() override;

    // 插件生命週期
    
    /**
     * @brief 從指定目錄載入所有插件
     * @param pluginPath 插件目錄路徑
     * @return 成功載入返回 true
     */
    bool loadPlugins(const QString& pluginPath);
    
     /**
      * @brief 載入單一插件
      * @param filePath 插件庫檔案路徑
      * @return 成功載入返回 true
      */
     bool loadPlugin(const QString& filePath);
     
     /**
      * @brief 取得已安裝插件的版本
      */
     QString pluginVersion(const QString& pluginId) const { QMutexLocker locker(&m_mutex); return m_pluginVersions.value(pluginId); }

    /**
     * @brief 從下載的壓縮包（zip）安裝插件並熱載入
     * @param pluginId 插件 ID
     * @param archiveData 下載的壓縮包位元組
     * @return 成功返回 true
     */
    bool installPluginFromArchive(const QString& pluginId, const QByteArray& archiveData, QWidget* parent = nullptr);

    /**
     * @brief 插件安裝目錄
     */
    QString pluginsInstallDir() const;

    /**
     * @brief 建立插件依賴圖（公開以便測試與外部排序使用）
     */
    bool buildDependencyGraph(const QList<QJsonObject>& pluginMetadata);

    /**
     * @brief 執行拓撲排序，回傳排序後的插件 ID 列表
     */
    QStringList topologicalSort();

    /**
     * @brief 檢查插件版本相容性
     */
    bool checkVersionCompatibility(const QJsonObject& metadata) const;

    /**
     * @brief 載入插件元數據
     * @param filePath 插件目錄路徑
     * @param metadata 輸出的元數據物件
     * @return 成功載入返回 true
     */
    bool loadPluginMetadata(const QString& filePath, QJsonObject& metadata);
    
    /**
     * @brief 卸載指定插件
     * @param id 插件 ID
     */
    void unloadPlugin(const QString& id);
    
    /**
     * @brief 卸載所有插件
     */
    void unloadAllPlugins();

     // 插件查詢
     
     /**
      * @brief 獲取所有已載入的插件
      * @return 插件列表
      */
     QList<ScripturaPlugin*> plugins() const;
     
     /**
      * @brief 獲取指定 ID 的插件
      * @param id 插件 ID
      * @return 插件指標，如果未找到則返回 nullptr
      */
     ScripturaPlugin* getPlugin(const QString& id) const;
     
     /**
      * @brief 檢查插件是否已載入
      * @param id 插件 ID
      * @return 已載入返回 true
      */
     bool isLoaded(const QString& id) const;

     // 允許清單（僅 GUI 安裝）
     
     /**
      * @brief 設定允許載入的插件 ID 清單
      * @param allowed 允許的插件 ID 集合
      */
     void setAllowedPlugins(const QSet<QString>& allowed);
     
     /**
      * @brief 新增允許載入的插件 ID
      * @param id 插件 ID
      */
     void addAllowedPlugin(const QString& id);
     
     /**
      * @brief 移除允許載入的插件 ID
      * @param id 插件 ID
      */
     void removeAllowedPlugin(const QString& id);
     
     /**
      * @brief 檢查插件是否在允許清單中
      * @param id 插件 ID
      * @return 在允許清單中返回 true
      */
     bool isAllowed(const QString& id) const;
     
     /**
      * @brief 獲取允許載入的插件 ID 清單
      * @return 允許的插件 ID 集合
      */
     QSet<QString> allowedPlugins() const;

     // 插件啟用/停用管理
     
     /**
      * @brief 停用插件（不刪除）
      * @param id 插件 ID
      * @return 成功返回 true
      */
     bool disablePlugin(const QString& id);
     
     /**
      * @brief 啟用插件
      * @param id 插件 ID
      * @return 成功返回 true
      */
     bool enablePlugin(const QString& id);
     
     /**
      * @brief 熱重載插件
      * @param id 插件 ID
      * @return 成功返回 true
      */
     bool reloadPlugin(const QString& id);
     
     /**
      * @brief 檢查插件是否已停用
      * @param id 插件 ID
      * @return 已停用返回 true
      */
     bool isDisabled(const QString& id) const;
     
     /**
      * @brief 獲取所有停用的插件 ID
      * @return 停用的插件 ID 集合
      */
     QSet<QString> disabledPlugins() const;
     
     /**
      * @brief 載入停用的插件列表
      */
     void loadDisabledPlugins();

     bool checkPermission(const QString& pluginId, Permission permission) const;
    void onPluginCrashed(const QString& pluginId, const CrashInfo& info);

     // 功能查詢
    
    /**
     * @brief 獲取支援特定功能的插件列表
     * @param feature 功能特徵
     * @return 插件列表
     */
    QList<ScripturaPlugin*> pluginsWithFeature(PluginFeature feature) const;

    // 事件系統
    
    /**
     * @brief 發佈事件到所有訂閱者
     * @param event 事件名稱
     * @param data 事件資料
     */
    void publishEvent(const QString& event, const QVariant& data = QVariant());
    
    /**
     * @brief 訂閱事件
     * @param event 事件名稱
     * @param callback 回調函數
     * @return 訂閱 ID
     */
    quint64 subscribeToEvent(const QString& event, 
                             std::function<void(const QVariant&)> callback);

    /**
     * @brief 訂閱事件 (綁定擁有者插件 ID)
     * @param event 事件名稱
     * @param callback 回調函數
     * @param ownerPluginId 擁有此訂閱的插件 ID，卸載時會自動移除
     * @return 訂閱 ID
     */
    quint64 subscribeToEvent(const QString& event, 
                             std::function<void(const QVariant&)> callback,
                             const QString& ownerPluginId);

signals:
    /**
     * @brief 插件載入成功信號
     * @param id 插件 ID
     */
    void pluginLoaded(const QString& id);
    
    /**
     * @brief 插件卸載信號
     * @param id 插件 ID
     */
    void pluginUnloaded(const QString& id);
    
    /**
     * @brief 插件錯誤信號
     * @param id 插件 ID
     * @param error 錯誤訊息
     */
    void pluginError(const QString& id, const QString& error);
    
    /**
     * @brief 插件不相容信號
     * @param id 插件 ID
     * @param coreVersion 當前 Core 版本
     */
    void pluginIncompatible(const QString& id, const QString& coreVersion);

 private:
    /**
     * @brief 檢查插件依賴
     * @param metadata 插件元數據
     * @return 依賴滿足返回 true
     */
    bool checkDependencies(const QJsonObject& metadata);
    
    /**
     * @brief 解析所有插件依賴
     * @return 成功解析返回 true
     */
    bool resolveDependencies();
    
    /**
     * @brief 初始化所有已載入的插件
     */
    void initializePlugins();
    
    /**
     * @brief 設定插件信號連接
     * @param plugin 插件指標
     */
    void setupPluginConnections(ScripturaPlugin* plugin);

    /**
     * @brief 內部載入實作（不acquire鎖，供已持有鎖的內部呼叫使用）
     * @param filePath 插件庫檔案路徑
     * @return 成功載入返回 true
     */
    bool loadPluginImpl(const QString& filePath);
    
    /**
     * @brief 內部卸載邏輯（不acquire鎖，供已持有鎖的內部呼叫使用）
     * @param id 插件 ID
     */
    void unloadPluginImpl(const QString& id);
    
    /**
     * @brief 根據 ID 載入插件
     * @param pluginId 插件 ID
     * @return 成功載入返回 true
     */
    bool loadPluginById(const QString& pluginId);


    /**
     * @brief 插件狀態列舉
     */
    enum class PluginState {
        NotLoaded,    ///< 未載入
        Loaded,       ///< 已載入
        Failed,       ///< 載入失敗
        Disabled      ///< 使用者手動停用
    };

    /**
     * @brief 插件資訊結構
     */
    struct PluginInfo {
        QString filePath;           ///< 插件目錄路徑
        QJsonObject metadata;       ///< 插件元數據
        QSharedPointer<QPluginLoader> loader; ///< Qt 插件載入器
        ScripturaPlugin* instance;    ///< 插件實例
        PluginState state;          ///< 插件狀態
        bool initialized;           ///< 是否已初始化
        QStringList dependencies;   ///< 依賴列表
        
        PluginInfo() : loader(nullptr), instance(nullptr), state(PluginState::NotLoaded), initialized(false) {}
    };
    
    /**
     * @brief 儲存停用的插件列表
     */
    void saveDisabledPlugins() const;

    /**
     * @brief 訂閱資訊
     */
    struct Subscription {
        quint64 id;
        std::function<void(const QVariant&)> callback;
    };

    QHash<QString, PluginInfo> m_plugins;                           ///< 插件註冊表
    QHash<QString, QStringList> m_dependencyGraph;                  ///< 依賴圖
    QHash<QString, QString> m_pluginPaths;                          ///< 插件ID -> 目錄路徑
    QHash<QString, QString> m_pluginVersions;                       ///< 插件ID -> 版本
    QHash<QString, QList<Subscription>> m_eventHandlers;            ///< 事件處理器
    QHash<QString, QList<quint64>> m_pluginSubscriptions;            ///< 插件ID -> 訂閱ID 列表
    QSet<QString> m_allowedPlugins;                                 ///< 允許載入的插件 ID
    QSet<QString> m_disabledPlugins;                                ///< 手動停用的插件 ID
    quint64 m_nextSubscriptionId = 1;
    PluginContext* m_context = nullptr;
    mutable QRecursiveMutex m_mutex;
    
    static const QString DISABLED_PLUGINS_FILE;                      ///< 停用插件列表檔案名稱
    static const QString DISABLED_PLUGINS_INI;                       ///< 停用插件列表 INI 檔案名稱（已棄用）
};

#endif // PLUGINMANAGER_H