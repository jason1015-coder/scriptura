#ifndef SERVICELOCATOR_H
#define SERVICELOCATOR_H

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>

/**
 * @file servicelocator.h
 * @brief 定義服務定位器，用於插件之間的服務註冊和查詢
 * 
 * ServiceLocator 實作了服務定位器模式，允許插件：
 * - 註冊服務實例
 * - 查詢已註冊的服務
 * - 取消註冊服務
 */

/**
 * @class ServiceLocator
 * @brief 服務定位器，管理服務實例的註冊和查詢
 * 
 * 這是一個單例模式的類別，提供線程安全的服務註冊機制。
 * 插件可以使用它來註冊服務，讓其他插件可以使用這些服務。
 */
class ServiceLocator : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 獲取 ServiceLocator 單例實例
     * @return ServiceLocator 單例指標
     */
    static ServiceLocator* instance();
    
    /**
     * @brief 刪除單例實例
     */
    static void destroyInstance();

    /**
     * @brief 註冊服務
     * @tparam T 服務類型
     * @param id 服務 ID
     * @param service 服務實例指標
     */
    template<typename T>
    void registerService(const QString& id, T* service);
    
    /**
     * @brief 獲取服務
     * @tparam T 服務類型
     * @param id 服務 ID
     * @return 服務實例指標，如果未找到則返回 nullptr
     */
    template<typename T>
    T* getService(const QString& id) const;
    
    /**
     * @brief 取消註冊服務
     * @param id 服務 ID
     */
    void unregisterService(const QString& id);
    
    /**
     * @brief 檢查服務是否已註冊
     * @param id 服務 ID
     * @return 已註冊返回 true
     */
    bool hasService(const QString& id) const;
    
    /**
     * @brief 獲取所有已註冊的服務 ID
     * @return 服務 ID 列表
     */
    QStringList registeredServices() const;

private:
    /**
     * @brief 私有建構函數 (單例模式)
     */
    ServiceLocator(QObject* parent = nullptr);
    
    /**
     * @brief 私有解構函數
     */
    ~ServiceLocator() override;
    
    // 禁止複製和賦值
    ServiceLocator(const ServiceLocator&) = delete;
    ServiceLocator& operator=(const ServiceLocator&) = delete;
    
    QHash<QString, QObject*> m_services;
    mutable QMutex m_mutex;
    
    static ServiceLocator* s_instance;
};

// 模板方法實作
template<typename T>
void ServiceLocator::registerService(const QString& id, T* service)
{
    QMutexLocker locker(&m_mutex);
    m_services[id] = service;
}

template<typename T>
T* ServiceLocator::getService(const QString& id) const
{
    QMutexLocker locker(&m_mutex);
    if (!m_services.contains(id)) {
        return nullptr;
    }
    return qobject_cast<T*>(m_services[id]);
}

#endif // SERVICELOCATOR_H