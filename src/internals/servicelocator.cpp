#include "servicelocator.h"
#include <QDebug>

// 靜態成員初始化
ServiceLocator* ServiceLocator::s_instance = nullptr;

static QMutex s_instanceMutex;

ServiceLocator* ServiceLocator::instance()
{
    QMutexLocker locker(&s_instanceMutex);
    if (!s_instance) {
        s_instance = new ServiceLocator();
    }
    return s_instance;
}

void ServiceLocator::destroyInstance()
{
    QMutexLocker locker(&s_instanceMutex);
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

ServiceLocator::ServiceLocator(QObject* parent)
    : QObject(parent)
    , m_rustSl(rust_service_locator_new())
{
}

ServiceLocator::~ServiceLocator()
{
    QMutexLocker locker(&m_mutex);
    if (m_rustSl) {
        rust_service_locator_free(m_rustSl);
        m_rustSl = nullptr;
    }
}

void ServiceLocator::unregisterService(const QString& id)
{
    QByteArray idBytes = id.toUtf8();
    rust_service_locator_unregister(m_rustSl, idBytes.constData());
}

bool ServiceLocator::hasService(const QString& id) const
{
    QByteArray idBytes = id.toUtf8();
    return rust_service_locator_has(m_rustSl, idBytes.constData());
}

QStringList ServiceLocator::registeredServices() const
{
    QStringList result;
    size_t len = 0;
    char** list = rust_service_locator_list(m_rustSl, &len);
    if (!list) return result;
    for (size_t i = 0; i < len; ++i) {
        result << QString::fromUtf8(list[i]);
    }
    rust_service_locator_free_list(list, len);
    return result;
}
