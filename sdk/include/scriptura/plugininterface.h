#ifndef PLUGININTERFACE_H
#define PLUGININTERFACE_H

#include <QtPlugin>
#include <QString>
#include <QStringList>
#include "pluginfeature.h"

/**
 * @file plugininterface.h
 * @brief 定義 Scriptura 插件系統的基礎介面
 * 
 * 所有 Scriptura 插件都必須實作此介面，
 * 它定義了插件的生命週期、元數據和功能查詢方法。
 */

class PluginContext;

/**
 * @class ScripturaPlugin
 * @brief 所有 Scriptura 插件的基礎抽象介面
 * 
 * 此介面定義了插件系統的核心契約，包括：
 * - 生命週期管理：initialize() 和 shutdown()
 * - 元數據提供：id(), name(), version(), author(), description()
 * - 依賴聲明：dependencies()
 * - 功能查詢：hasFeature()
 */
class ScripturaPlugin : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 建構函數
     * @param parent 父物件
     */
    explicit ScripturaPlugin(QObject* parent = nullptr)
        : QObject(parent)
    {
    }
    
    /**
     * @brief 虛擬解構函數
     */
    virtual ~ScripturaPlugin() = default;
    
    /**
     * @brief 初始化插件
     * @param context 插件上下文，提供對核心服務的訪問
     * @return 初始化成功返回 true，失敗返回 false
     * 
     * 這個方法在插件加載後被調用，用於：
     * - 註冊服務
     * - 創建 UI 元件
     * - 連接信號槽
     * - 初始化插件狀態
     */
    virtual bool initialize(PluginContext* context) = 0;
    
    /**
     * @brief 關閉插件
     * 
     * 這個方法在插件卸載前被調用，用於：
     * - 清理資源
     * - 斷開連接
     * - 保存狀態
     */
    virtual void shutdown() = 0;
    
    /**
     * @brief 獲取插件唯一識別符
     * @return 插件 ID (例如: "com.scriptura.markdown")
     */
    virtual QString id() const = 0;
    
    /**
     * @brief 獲取插件顯示名稱
     * @return 插件名稱
     */
    virtual QString name() const = 0;
    
    /**
     * @brief 獲取插件版本號
     * @return 版本字串 (語義化版本格式)
     */
    virtual QString version() const = 0;
    
    /**
     * @brief 獲取插件作者資訊
     * @return 作者名稱
     */
    virtual QString author() const = 0;
    
    /**
     * @brief 獲取插件描述
     * @return 插件功能描述
     */
    virtual QString description() const = 0;
    
    /**
     * @brief 獲取插件依賴列表
     * @return 必需依賴的插件 ID 列表
     */
    virtual QStringList dependencies() const = 0;
    
    /**
     * @brief 檢查插件是否支援特定功能
     * @param feature 要檢查的功能特徵
     * @return 支援返回 true，不支援返回 false
     */
    virtual bool hasFeature(PluginFeature feature) const = 0;
};

/**
 * @brief Qt 插件介面註冊
 * 
 * 這個宏註冊了 ScripturaPlugin 介面，使其可以被 Qt 的插件加載機制發現。
 * 介面 ID 用於唯一識別此介面版本。
 */
Q_DECLARE_INTERFACE(ScripturaPlugin, "com.scriptura.plugin/1.0")

#endif // PLUGININTERFACE_H