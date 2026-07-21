#ifndef PLUGINNOTIFICATIONAPI_H
#define PLUGINNOTIFICATIONAPI_H

#include <QObject>
#include <QString>

class MainWindow;

/**
 * @file notificationapi.h
 * @brief Plugin Notification API — toasts, status messages, progress indicators
 *
 * Allows plugins to surface non-blocking notifications, progress bars,
 * and status messages to the user.
 */
class PluginNotificationApi : public QObject
{
    Q_OBJECT

public:
    explicit PluginNotificationApi(MainWindow *mainWindow, QObject *parent = nullptr);
    ~PluginNotificationApi() override;

    // ── Toast Notifications ────────────────────────────────────

    /**
     * @brief Show an informational toast notification.
     * @param title       Short title
     * @param message     Body text
     * @param durationMs  Auto-dismiss time (ms); 0 = sticky
     */
    void showInfo(const QString &title, const QString &message, int durationMs = 3000);

    /**
     * @brief Show a warning toast notification.
     * @param title       Short title
     * @param message     Body text
     * @param durationMs  Auto-dismiss time (ms); 0 = sticky
     */
    void showWarning(const QString &title, const QString &message, int durationMs = 5000);

    /**
     * @brief Show an error toast notification.
     * @param title       Short title
     * @param message     Body text
     * @param durationMs  Auto-dismiss time (ms); 0 = sticky
     */
    void showError(const QString &title, const QString &message, int durationMs = 8000);

    // ── Status Bar Messages ────────────────────────────────────

    /**
     * @brief Show a temporary message on the status bar.
     * @param message    Text to display
     * @param timeoutMs  Duration in milliseconds (0 = sticky)
     */
    void showStatusMessage(const QString &message, int timeoutMs = 3000);

    // ── Progress Indicator ─────────────────────────────────────

    /**
     * @brief Show a progress indicator.
     * @param taskId   Unique identifier for the task
     * @param label    Human-readable label
     * @param minimum  Minimum value (usually 0)
     * @param maximum  Maximum value (-1 for indeterminate)
     */
    void showProgress(const QString &taskId, const QString &label,
                      int minimum = 0, int maximum = 100);

    /**
     * @brief Update progress value for a task.
     * @param taskId  Task identifier
     * @param value   Current progress value
     */
    void updateProgress(const QString &taskId, int value);

    /**
     * @brief Hide and remove a progress indicator.
     * @param taskId  Task identifier
     */
    void hideProgress(const QString &taskId);

private:
    MainWindow *m_mainWindow;
};

#endif // PLUGINNOTIFICATIONAPI_H
