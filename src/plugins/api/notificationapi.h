#ifndef SRC_PLUGINNOTIFICATIONAPI_H
#define SRC_PLUGINNOTIFICATIONAPI_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QProgressBar>
#include <QLabel>

class MainWindow;

class PluginNotificationApi : public QObject
{
    Q_OBJECT

public:
    explicit PluginNotificationApi(MainWindow *mainWindow, QObject *parent = nullptr);
    ~PluginNotificationApi() override;

    void showInfo(const QString &title, const QString &message, int durationMs = 3000);
    void showWarning(const QString &title, const QString &message, int durationMs = 5000);
    void showError(const QString &title, const QString &message, int durationMs = 8000);

    void showStatusMessage(const QString &message, int timeoutMs = 3000);

    void showProgress(const QString &taskId, const QString &label,
                      int minimum = 0, int maximum = 100);
    void updateProgress(const QString &taskId, int value);
    void hideProgress(const QString &taskId);

private:
    struct ProgressEntry {
        QWidget *container = nullptr;
        QLabel *label = nullptr;
        QProgressBar *bar = nullptr;
    };

    void showToast(const QString &title, const QString &message,
                   const QString &styleClass, int durationMs);

    MainWindow *m_mainWindow;
    QHash<QString, ProgressEntry> m_progressBars;
};

#endif // SRC_PLUGINNOTIFICATIONAPI_H
