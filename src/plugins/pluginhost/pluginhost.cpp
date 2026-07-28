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
#include <QTimer>
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

        QJsonObject request = doc.object();
        int id = request["id"].toInt();
        QString method = request["method"].toString();
        QJsonObject params = request["params"].toObject();

        QJsonObject response;
        if (method == "ping") {
            response = handlePing(params);
        } else if (method == "shutdown") {
            response = handleShutdown(params);
        } else {
            response = handleRequest(request);
        }

        response["id"] = id;
        send(response);
    }

    QJsonObject handlePing(const QJsonObject &params)
    {
        Q_UNUSED(params);
        QJsonObject result;
        result["pong"] = true;
        result["pid"] = QCoreApplication::applicationPid();
        return result;
    }

    QJsonObject handleShutdown(const QJsonObject &params)
    {
        Q_UNUSED(params);
        QJsonObject result;
        result["success"] = true;
        QTimer::singleShot(100, qApp, &QCoreApplication::quit);
        return result;
    }

    QJsonObject handleRequest(const QJsonObject &request)
    {
        Q_UNUSED(request);
        QJsonObject result;
        result["error"] = "Not implemented";
        return result;
    }

    void send(const QJsonObject &response)
    {
        QByteArray data = QJsonDocument(response).toJson(QJsonDocument::Compact);
        QByteArray frame = LengthPrefixedFramer::frame(data);
        fwrite(frame.constData(), 1, frame.size(), stdout);
        fflush(stdout);
    }

    void sendError(int id, const QString &message)
    {
        QJsonObject response;
        response["id"] = id;
        QJsonObject error;
        error["code"] = -1;
        error["message"] = message;
        response["error"] = error;
        send(response);
    }

    LengthPrefixedFramer *m_framer = nullptr;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    PluginHost host;
    return host.run();
}

#include "pluginhost.moc"
