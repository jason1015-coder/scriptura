#include "applicationcontext.h"
#include "mainwindow.h"
#include "plugins/api/uiapi.h"
#include "plugins/api/notificationapi.h"

ApplicationContext::ApplicationContext(MainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_settings(new QSettings(this))
    , m_uiApi(mainWindow ? new PluginUIApi(mainWindow, this) : nullptr)
    , m_notificationApi(mainWindow ? new PluginNotificationApi(mainWindow, this) : nullptr)
{
}

ApplicationContext::~ApplicationContext() = default;

QString ApplicationContext::currentProjectPath() const
{
    if (!m_mainWindow) return QString();
    return m_mainWindow->currentProjectPath();
}

void ApplicationContext::notify(const QString& event, const QVariant& data)
{
    Q_UNUSED(event)
    Q_UNUSED(data)
    // Event bus integration through Rust backend
}

quint64 ApplicationContext::subscribe(const QString& event, std::function<void(const QVariant&)> callback)
{
    Q_UNUSED(event)
    Q_UNUSED(callback)
    return 0;
}

void ApplicationContext::unsubscribe(const QString& event, quint64 subscriptionId)
{
    Q_UNUSED(event)
    Q_UNUSED(subscriptionId)
}
