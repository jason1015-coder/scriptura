#include "eventbus.h"
#include <QDebug>

// 靜態成員初始化
EventBus* EventBus::s_instance = nullptr;

static QMutex s_instanceMutex;

EventBus* EventBus::instance()
{
    QMutexLocker locker(&s_instanceMutex);
    if (!s_instance) {
        s_instance = new EventBus();
    }
    return s_instance;
}

void EventBus::destroyInstance()
{
    QMutexLocker locker(&s_instanceMutex);
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

EventBus::EventBus(QObject* parent)
    : QObject(parent)
{
}

EventBus::~EventBus()
{
    QMutexLocker locker(&m_mutex);
    m_subscribers.clear();
}

void EventBus::publish(const QString& event, const QVariant& data)
{
    QList<Subscription> callbacks;
    
    {
        QMutexLocker locker(&m_mutex);
        if (!m_subscribers.contains(event)) {
            return;
        }
        callbacks = m_subscribers[event];  // Copy list
    }  // Release lock here
    
    // Call callbacks outside the lock to avoid deadlocks
    for (const auto& subscription : callbacks) {
        if (subscription.hasReceiver && subscription.receiver.isNull()) {
            continue;
        }
        try {
            subscription.callback(data);
        } catch (const std::exception& e) {
            qWarning() << "EventBus: Exception in callback for event" << event << ":" << e.what();
        } catch (...) {
            qWarning() << "EventBus: Unknown exception in callback for event" << event;
        }
    }
}

EventBus::SubscriptionId EventBus::subscribe(const QString& event, 
                                             std::function<void(const QVariant&)> callback,
                                             Qt::ConnectionType type)
{
    // Backward compat: no receiver means no lifecycle binding
    return subscribe(event, nullptr, std::move(callback), type);
}

EventBus::SubscriptionId EventBus::subscribe(const QString& event,
                                             QObject* receiver,
                                             std::function<void(const QVariant&)> callback,
                                             Qt::ConnectionType type)
{
    Q_UNUSED(type); // Reserved for future Qt signal connection use

    QMutexLocker locker(&m_mutex);

    const SubscriptionId id = m_nextId++;
    Subscription sub;
    sub.id = id;
    sub.callback = std::move(callback);
    sub.receiver = receiver;
    sub.hasReceiver = (receiver != nullptr);
    m_subscribers[event].append(sub);

    // Bind receiver lifecycle: auto-unsubscribe when destroyed
    if (receiver) {
        connect(receiver, &QObject::destroyed, this, [this, event, id]() {
            unsubscribe(event, id);
        });
    }

    return id;
}

void EventBus::unsubscribeReceiver(QObject* receiver)
{
    if (!receiver) return;

    QMutexLocker locker(&m_mutex);

    for (auto it = m_subscribers.begin(); it != m_subscribers.end();) {
        QList<Subscription>& subscriptions = it.value();
        for (auto sit = subscriptions.begin(); sit != subscriptions.end();) {
            if (sit->receiver.data() == receiver) {
                sit = subscriptions.erase(sit);
            } else {
                ++sit;
            }
        }
        if (subscriptions.isEmpty()) {
            it = m_subscribers.erase(it);
        } else {
            ++it;
        }
    }
}

void EventBus::unsubscribe(const QString& event, SubscriptionId subscriptionId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_subscribers.contains(event)) {
        return;
    }
    
    auto& subscriptions = m_subscribers[event];
    for (auto it = subscriptions.begin(); it != subscriptions.end(); ++it) {
        if (it->id == subscriptionId) {
            subscriptions.erase(it);
            break;
        }
    }
    
    if (subscriptions.isEmpty()) {
        m_subscribers.remove(event);
    }
}

bool EventBus::hasSubscribers(const QString& event) const
{
    QMutexLocker locker(&m_mutex);
    return m_subscribers.contains(event) && !m_subscribers[event].isEmpty();
}
