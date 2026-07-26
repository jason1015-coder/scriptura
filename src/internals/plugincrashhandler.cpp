#include "plugincrashhandler.h"
#include "rust_backend.h"
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QMetaObject>

PluginCrashHandler::PluginCrashHandler(QObject* parent)
    : QObject(parent)
    , m_rustH(rust_crash_handler_new())
    , m_crashLogPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugin_crashes.log")
{
    QDir().mkpath(QFileInfo(m_crashLogPath).absolutePath());

    // Register the crash callback: when Rust detects/reports a crash, emit Qt signal
    rust_crash_handler_on_crash(m_rustH, &PluginCrashHandler::onCrashCb, this);
}

PluginCrashHandler::~PluginCrashHandler()
{
    if (m_rustH) {
        rust_crash_handler_free(m_rustH);
        m_rustH = nullptr;
    }
    qDeleteAll(m_pluginProcesses);
    m_pluginProcesses.clear();
}

void PluginCrashHandler::registerPluginProcess(const QString& pluginId, QProcess* process)
{
    if (!process) return;

    if (m_pluginProcesses.contains(pluginId)) {
        delete m_pluginProcesses.take(pluginId);
    }

    m_pluginProcesses[pluginId] = process;

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, pluginId](int exitCode, QProcess::ExitStatus exitStatus) {
        Q_UNUSED(exitStatus);
        if (exitCode != 0) {
            handleCrash(pluginId);
        }
    });
}

void PluginCrashHandler::handleCrash(const QString& pluginId)
{
    QByteArray idBytes = pluginId.toUtf8();
    QString errorStr = QStringLiteral("Process crashed");
    QByteArray errorBytes = errorStr.toUtf8();
    rust_crash_handler_report_crash(m_rustH, idBytes.constData(), errorBytes.constData());

    // Build CrashInfo for the Qt side
    CrashInfo info;
    info.pluginId = pluginId;
    info.timestamp = QDateTime::currentDateTime();
    info.errorType = errorStr;
    info.stackTrace = QString();
    info.autoDisabled = true;

    m_crashHistory.prepend(info);
    if (m_crashHistory.size() > 100) {
        m_crashHistory.removeLast();
    }

    // Log to file
    QString logEntry = QString("[%1] Plugin crashed: %2\n")
                           .arg(info.timestamp.toString(Qt::ISODate))
                           .arg(pluginId);
    QFile logFile(m_crashLogPath);
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out << logEntry;
        logFile.close();
    }

    disablePlugin(pluginId);
    emit pluginCrashed(pluginId, info);
    qWarning() << "Plugin crashed:" << pluginId << "at" << info.timestamp;
}

void PluginCrashHandler::disablePlugin(const QString& pluginId)
{
    m_disabledPlugins[pluginId] = true;
}

QList<CrashInfo> PluginCrashHandler::recentCrashes(int limit)
{
    if (limit <= 0 || limit >= m_crashHistory.size()) {
        return m_crashHistory;
    }
    return m_crashHistory.mid(0, limit);
}

bool PluginCrashHandler::isPluginDisabled(const QString& pluginId) const
{
    return m_disabledPlugins.value(pluginId, false);
}

void PluginCrashHandler::enablePlugin(const QString& pluginId)
{
    m_disabledPlugins.remove(pluginId);
}

void PluginCrashHandler::onCrashCb(const char* pluginId, const char* error, void* userData)
{
    auto* self = static_cast<PluginCrashHandler*>(userData);
    if (!self) return;

    QString id = QString::fromUtf8(pluginId);
    QString err = QString::fromUtf8(error);

    QMetaObject::invokeMethod(self, [self, id, err]() {
        CrashInfo info;
        info.pluginId = id;
        info.timestamp = QDateTime::currentDateTime();
        info.errorType = err;
        info.stackTrace = QString();
        info.autoDisabled = true;
        self->m_crashHistory.prepend(info);
        if (self->m_crashHistory.size() > 100) {
            self->m_crashHistory.removeLast();
        }
        self->disablePlugin(id);
        emit self->pluginCrashed(id, info);
    }, Qt::QueuedConnection);
}
