#ifndef NOTIFICATIONCENTER_H
#define NOTIFICATIONCENTER_H

#include <QObject>
#include <QList>
#include <QDateTime>
#include <QString>

/**
 * Represents a single notification.
 */
struct Notification {
    int id;
    QString title;
    QString message;
    QString type;       // "info", "warning", "error", "success"
    QDateTime timestamp;
    QString action;     // Optional action string
    bool read = false;
};

/**
 * Central notification manager that stores notification history,
 * provides filtering, search, and a UI panel.
 */
class NotificationCenter : public QObject
{
    Q_OBJECT
public:
    explicit NotificationCenter(QObject *parent = nullptr);

    // Add a notification
    int addNotification(const QString &title, const QString &message,
                        const QString &type = "info", const QString &action = QString());

    // Mark notification as read
    void markAsRead(int id);

    // Mark all as read
    void markAllAsRead();

    // Clear all notifications
    void clearAll();

    // Remove a specific notification
    void removeNotification(int id);

    // Get all notifications
    QList<Notification> notifications() const { return m_notifications; }

    // Get unread count
    int unreadCount() const;

    // Get notifications filtered by type
    QList<Notification> notificationsByType(const QString &type) const;

    // Search notifications by text
    QList<Notification> searchNotifications(const QString &query) const;

    // Get recent N notifications
    QList<Notification> recentNotifications(int count = 50) const;

    // Enable/disable persistence
    void setPersistenceEnabled(bool enabled) { m_persistenceEnabled = enabled; }
    bool persistenceEnabled() const { return m_persistenceEnabled; }

    // Save/load from settings
    void saveToSettings();
    void loadFromSettings();

signals:
    void notificationAdded(const Notification &notification);
    void notificationRemoved(int id);
    void notificationsCleared();
    void unreadCountChanged(int count);

private:
    QList<Notification> m_notifications;
    int m_nextId = 1;
    bool m_persistenceEnabled = true;
    int m_maxNotifications = 500;
};

#endif // NOTIFICATIONCENTER_H
