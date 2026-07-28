#include "notificationcenter.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

NotificationCenter::NotificationCenter(QObject *parent) : QObject(parent)
{
    loadFromSettings();
}

int NotificationCenter::addNotification(const QString &title, const QString &message,
                                         const QString &type, const QString &action)
{
    Notification notif;
    notif.id = m_nextId++;
    notif.title = title;
    notif.message = message;
    notif.type = type;
    notif.timestamp = QDateTime::currentDateTime();
    notif.action = action;
    notif.read = false;

    m_notifications.prepend(notif);

    // Trim old notifications
    while (m_notifications.size() > m_maxNotifications) {
        m_notifications.removeLast();
    }

    emit notificationAdded(notif);
    emit unreadCountChanged(unreadCount());

    if (m_persistenceEnabled) {
        saveToSettings();
    }

    return notif.id;
}

void NotificationCenter::markAsRead(int id)
{
    for (auto &notif : m_notifications) {
        if (notif.id == id && !notif.read) {
            notif.read = true;
            emit unreadCountChanged(unreadCount());
            if (m_persistenceEnabled) saveToSettings();
            return;
        }
    }
}

void NotificationCenter::markAllAsRead()
{
    bool changed = false;
    for (auto &notif : m_notifications) {
        if (!notif.read) {
            notif.read = true;
            changed = true;
        }
    }
    if (changed) {
        emit unreadCountChanged(0);
        if (m_persistenceEnabled) saveToSettings();
    }
}

void NotificationCenter::clearAll()
{
    m_notifications.clear();
    emit notificationsCleared();
    emit unreadCountChanged(0);
    if (m_persistenceEnabled) saveToSettings();
}

void NotificationCenter::removeNotification(int id)
{
    for (int i = 0; i < m_notifications.size(); ++i) {
        if (m_notifications[i].id == id) {
            m_notifications.removeAt(i);
            emit notificationRemoved(id);
            emit unreadCountChanged(unreadCount());
            if (m_persistenceEnabled) saveToSettings();
            return;
        }
    }
}

int NotificationCenter::unreadCount() const
{
    int count = 0;
    for (const auto &notif : m_notifications) {
        if (!notif.read) count++;
    }
    return count;
}

QList<Notification> NotificationCenter::notificationsByType(const QString &type) const
{
    QList<Notification> result;
    for (const auto &notif : m_notifications) {
        if (notif.type == type) result.append(notif);
    }
    return result;
}

QList<Notification> NotificationCenter::searchNotifications(const QString &query) const
{
    QList<Notification> result;
    for (const auto &notif : m_notifications) {
        if (notif.title.contains(query, Qt::CaseInsensitive) ||
            notif.message.contains(query, Qt::CaseInsensitive)) {
            result.append(notif);
        }
    }
    return result;
}

QList<Notification> NotificationCenter::recentNotifications(int count) const
{
    return m_notifications.mid(0, qMin(count, m_notifications.size()));
}

void NotificationCenter::saveToSettings()
{
    QSettings settings;
    QJsonArray arr;
    for (const auto &notif : m_notifications) {
        QJsonObject obj;
        obj["id"] = notif.id;
        obj["title"] = notif.title;
        obj["message"] = notif.message;
        obj["type"] = notif.type;
        obj["timestamp"] = notif.timestamp.toString(Qt::ISODate);
        obj["action"] = notif.action;
        obj["read"] = notif.read;
        arr.append(obj);
    }
    settings.setValue("notifications/data", QString(QJsonDocument(arr).toJson()));
}

void NotificationCenter::loadFromSettings()
{
    QSettings settings;
    QString data = settings.value("notifications/data").toString();
    if (data.isEmpty()) return;

    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    QJsonArray arr = doc.array();

    m_notifications.clear();
    for (const QJsonValue &v : arr) {
        QJsonObject obj = v.toObject();
        Notification notif;
        notif.id = obj["id"].toInt();
        notif.title = obj["title"].toString();
        notif.message = obj["message"].toString();
        notif.type = obj["type"].toString();
        notif.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
        notif.action = obj["action"].toString();
        notif.read = obj["read"].toBool();
        m_notifications.append(notif);
        if (notif.id >= m_nextId) m_nextId = notif.id + 1;
    }

    emit unreadCountChanged(unreadCount());
}
