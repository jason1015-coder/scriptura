#include "pluginhost.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <cstdio>

PluginHost::PluginHost(QObject *parent)
    : QObject(parent)
    , m_framer(new LengthPrefixedFramer(this))
{
}

bool PluginHost::loadPlugin(const QString &pluginDir)
{
    QDir dir(pluginDir);
    if (!dir.exists())
        return false;

    QStringList candidates = dir.entryList(QStringList() << "*.so" << "*.dll" << "*.dylib", QDir::Files);
    if (candidates.isEmpty())
        return false;

    QString libraryPath = dir.absoluteFilePath(candidates.first());
    m_loader = new QPluginLoader(libraryPath, this);

    m_pluginInstance = m_loader->instance();
    if (!m_pluginInstance) {
        emit error(QObject::tr("Failed to load plugin: %1").arg(m_loader->errorString()));
        return false;
    }

    return true;
}

void PluginHost::run()
{
    m_running = true;
    emit started();

    while (m_running) {
        char buf[4096];
        size_t n = fread(buf, 1, sizeof(buf), stdin);
        if (n == 0)
            break;

        m_framer->append(QByteArray(buf, static_cast<int>(n)));

        while (m_framer->canReadMessage()) {
            QByteArray msg = m_framer->readMessage();
            if (msg.isEmpty())
                break;

            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(msg, &parseError);
            if (parseError.error != QJsonParseError::NoError) {
                emit error(QObject::tr("JSON parse error: %1").arg(parseError.errorString()));
                continue;
            }

            QJsonObject request = doc.object();
            QJsonObject response = handleRequest(request);
            response["jsonrpc"] = "2.0";
            if (request.contains("id"))
                response["id"] = request["id"];

            QByteArray out = QJsonDocument(response).toJson(QJsonDocument::Compact);
            QByteArray frame = LengthPrefixedFramer::frame(out);
            fwrite(frame.constData(), 1, frame.size(), stdout);
            fflush(stdout);
        }
    }
}

void PluginHost::shutdown()
{
    m_running = false;
    emit stopped();
}

void PluginHost::onStdinReadyRead()
{
}

void PluginHost::onPluginCrashed(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)
    emit error(QObject::tr("Plugin process crashed"));
    shutdown();
}

QJsonObject PluginHost::handleRequest(const QJsonObject &request)
{
    QString method = request["method"].toString();

    if (method == "ping") {
        return handlePing(request["params"].toObject());
    } else if (method == "shutdown") {
        return handleShutdown(request["params"].toObject());
    } else if (method == "plugin.info") {
        QJsonObject result;
        result["loaded"] = m_pluginInstance != nullptr;
        if (m_loader) {
            result["filePath"] = m_loader->fileName();
            result["error"] = m_loader->errorString();
        }
        return result;
    }

    QJsonObject error;
    error["code"] = -32601;
    error["message"] = QObject::tr("Method not found: %1").arg(method);
    return {{"error", error}};
}

QJsonObject PluginHost::handlePing(const QJsonObject &params)
{
    Q_UNUSED(params)
    return {{"result", "pong"}};
}

QJsonObject PluginHost::handleShutdown(const QJsonObject &params)
{
    Q_UNUSED(params)
    QJsonObject result;
    result["result"] = QJsonObject();
    shutdown();
    return result;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (argc < 2) {
        qWarning() << "Usage: scriptura_pluginhost <plugin-directory>";
        return 1;
    }

    QString pluginDir = QString::fromLocal8Bit(argv[1]);

    PluginHost host;
    if (!host.loadPlugin(pluginDir)) {
        qWarning() << "Failed to load plugin from" << pluginDir;
        return 1;
    }

    QObject::connect(&host, &PluginHost::error, [](const QString &msg) {
        qWarning() << "PluginHost error:" << msg;
    });

    host.run();

    return 0;
}
