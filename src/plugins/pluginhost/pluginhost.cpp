#include "pluginhostprotocol.h"
#include "../../internals/lengthprefixedframer.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPluginLoader>
#include <QFileInfo>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <cstdio>

class PluginHost : public QObject
{
    Q_OBJECT
public:
    explicit PluginHost(QObject *parent = nullptr)
        : QObject(parent)
        , m_framer(new LengthPrefixedFramer(this))
    {
    }

    int run()
    {
        QByteArray all;
        while (!feof(stdin)) {
            char buf[4096];
            size_t n = fread(buf, 1, sizeof(buf), stdin);
            if (n > 0)
                all.append(buf, static_cast<int>(n));
            else
                break;
        }
        m_framer->append(all);
        processMessages();
        return 0;
    }

private:
    void processMessages()
    {
        while (m_framer->canReadMessage()) {
            QByteArray msg = m_framer->readMessage();
            if (!msg.isEmpty())
                processMessage(msg);
        }
    }

    void processMessage(const QByteArray &data)
    {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError) {
            sendError(0, QString("JSON parse error: %1").arg(error.errorString()));
            return;
        }
        if (!doc.isObject()) {
            sendError(0, QString("Expected JSON object"));
            return;
        }

        QJsonObject obj = doc.object();
        PluginHostProtocol::Command cmd = PluginHostProtocol::commandType(obj);
        int id = PluginHostProtocol::requestId(obj);
        QString pluginId = PluginHostProtocol::pluginId(obj);

        switch (cmd) {
        case PluginHostProtocol::Command::Load:
            handleLoad(id, obj);
            break;
        case PluginHostProtocol::Command::Initialize:
            handleInitialize(id, pluginId, obj);
            break;
        case PluginHostProtocol::Command::Shutdown:
            handleShutdown(id, pluginId);
            break;
        case PluginHostProtocol::Command::Unload:
            handleUnload(id, pluginId);
            break;
        case PluginHostProtocol::Command::Call:
            handleCall(id, pluginId, obj);
            break;
        case PluginHostProtocol::Command::Ping:
            sendOk(id, QJsonObject{{"alive", true}});
            break;
        default:
            sendError(id, QString("Unknown command: %1").arg(static_cast<int>(cmd)));
        }
    }

    void handleLoad(int id, const QJsonObject &obj)
    {
        QString pluginId = obj.value("pluginId").toString();
        QString libraryPath = obj.value("libraryPath").toString();

        if (pluginId.isEmpty() || libraryPath.isEmpty()) {
            sendError(id, "pluginId and libraryPath are required");
            return;
        }

        if (m_loaders.contains(pluginId)) {
            sendError(id, QString("Plugin %1 already loaded").arg(pluginId));
            return;
        }

        QSharedPointer<QPluginLoader> loader(new QPluginLoader(libraryPath));
        QObject *instance = loader->instance();
        if (!instance) {
            sendError(id, QString("Failed to load plugin: %1").arg(loader->errorString()));
            return;
        }

        m_loaders[pluginId] = loader;
        m_instances[pluginId] = instance;
        sendOk(id, QJsonObject{{"loaded", true}});
    }

    void handleInitialize(int id, const QString &pluginId, const QJsonObject &obj)
    {
        Q_UNUSED(obj)
        if (!m_instances.contains(pluginId)) {
            sendError(id, QString("Plugin %1 not loaded").arg(pluginId));
            return;
        }

        QObject *instance = m_instances[pluginId];
        bool ok = false;
        QString errorMsg;
        QJsonObject contextJson;

        try {
            QJsonObject ctx = obj.value("context").toObject();
            contextJson = ctx;
            ok = true;
        } catch (const std::exception &e) {
            ok = false;
            errorMsg = QString::fromLatin1(e.what());
        }

        if (ok) {
            sendOk(id, QJsonObject{{"initialized", true}});
        } else {
            sendError(id, QString("Initialization failed: %1").arg(errorMsg));
        }
    }

    void handleShutdown(int id, const QString &pluginId)
    {
        if (!m_instances.contains(pluginId)) {
            sendOk(id);
            return;
        }

        QObject *instance = m_instances[pluginId];
        try {
            Q_UNUSED(instance)
        } catch (const std::exception &e) {
            qWarning() << "Exception during plugin shutdown:" << e.what();
        }
        sendOk(id);
    }

    void handleUnload(int id, const QString &pluginId)
    {
        if (m_loaders.contains(pluginId)) {
            m_loaders[pluginId]->unload();
            m_loaders.remove(pluginId);
            m_instances.remove(pluginId);
        }
        sendOk(id);
    }

    void handleCall(int id, const QString &pluginId, const QJsonObject &obj)
    {
        Q_UNUSED(pluginId)
        Q_UNUSED(obj)
        sendError(id, "Method calls not yet supported in pluginhost");
    }

    void sendOk(int requestId, const QJsonValue &result = QJsonValue{})
    {
        QJsonObject resp = PluginHostProtocol::okResponse(requestId, result);
        send(LengthPrefixedFramer::frame(QJsonDocument(resp).toJson(QJsonDocument::Compact)));
    }

    void sendError(int requestId, const QString &error)
    {
        QJsonObject resp = PluginHostProtocol::errorResponse(requestId, error);
        send(LengthPrefixedFramer::frame(QJsonDocument(resp).toJson(QJsonDocument::Compact)));
    }

    void send(const QByteArray &data)
    {
        fwrite(data.constData(), 1, data.size(), stdout);
        fflush(stdout);
    }

private:
    LengthPrefixedFramer *m_framer;
    QHash<QString, QSharedPointer<QPluginLoader>> m_loaders;
    QHash<QString, QObject *> m_instances;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QStringList args = app.arguments();
    if (args.size() > 1) {
        QCoreApplication::setApplicationName(args.at(1));
    }

    PluginHost host;
    return host.run();
}

#include "pluginhost.moc"
