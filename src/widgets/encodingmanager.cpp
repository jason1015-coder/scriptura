#include "encodingmanager.h"
#include "rust_adapter.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QStringDecoder>

EncodingManager::EncodingManager(QObject *parent) : QObject(parent)
{
    initEncodings();
}

void EncodingManager::initEncodings()
{
    // Delegate the supported-encoding list to the Rust backend.
    char *json = rust_encoding_supported();
    if (json) {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json));
        rust_free_string(json);
        if (doc.isArray()) {
            const QJsonArray arr = doc.array();
            if (!arr.isEmpty()) {
                for (const QJsonValue &v : arr) {
                    QJsonObject o = v.toObject();
                    QString name = o["name"].toString();
                    QString display = o["display"].toString();
                    if (name.isEmpty()) continue;
                    m_encodings[name] = {name, display, name.startsWith("UTF")};
                }
                return;
            }
        }
    }

    // Fallback list if FFI is unavailable
    m_encodings["UTF-8"] = {"UTF-8", "Unicode (UTF-8)", true};
    m_encodings["UTF-16LE"] = {"UTF-16LE", "Unicode (UTF-16 LE)", true};
    m_encodings["UTF-16BE"] = {"UTF-16BE", "Unicode (UTF-16 BE)", true};
    m_encodings["ISO-8859-1"] = {"ISO-8859-1", "Western (ISO-8859-1)", false};
    m_encodings["Windows-1252"] = {"Windows-1252", "Western (Windows-1252)", false};
    m_encodings["Shift-JIS"] = {"Shift-JIS", "Japanese (Shift-JIS)", false};
    m_encodings["GB2312"] = {"GB2312", "Chinese (GB2312)", false};
    m_encodings["EUC-KR"] = {"EUC-KR", "Korean (EUC-KR)", false};
    m_encodings["ISO-8859-15"] = {"ISO-8859-15", "Western European (ISO-8859-15)", false};
    m_encodings["KOI8-R"] = {"KOI8-R", "Russian (KOI8-R)", false};
}

QString EncodingManager::detectEncoding(const QString &filePath) const
{
    // Delegate BOM detection and UTF-8 validation to the Rust backend.
    QByteArray pathBytes = filePath.toUtf8();
    char *result = rust_encoding_detect(pathBytes.constData());
    if (!result) return "UTF-8";
    QString enc = QString::fromUtf8(result);
    rust_free_string(result);
    return enc;
}

QStringList EncodingManager::supportedEncodings() const
{
    return m_encodings.keys();
}

QString EncodingManager::encodingDisplayName(const QString &encoding) const
{
    return m_encodings.value(encoding).displayName;
}

QString EncodingManager::readFileWithEncoding(const QString &filePath, const QString &encoding) const
{
    // File I/O stays in C++: the Rust encoding engine's read/write are
    // UTF-8-only, while Qt's QStringConverter handles all encodings.
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return QString();

    QByteArray data = file.readAll();
    file.close();

    auto enc = QStringConverter::encodingForName(encoding.toUtf8());
    auto decoder = QStringDecoder(enc.value_or(QStringConverter::Utf8));
    return decoder(data);
}

bool EncodingManager::writeFileWithEncoding(const QString &filePath, const QString &content, const QString &encoding)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    auto enc = QStringConverter::encodingForName(encoding.toUtf8());
    auto encoder = QStringEncoder(enc.value_or(QStringConverter::Utf8));
    file.write(encoder(content));
    file.close();
    return true;
}

bool EncodingManager::convertEncoding(const QString &filePath, const QString &fromEncoding, const QString &toEncoding)
{
    QString content = readFileWithEncoding(filePath, fromEncoding);
    if (content.isEmpty()) return false;
    return writeFileWithEncoding(filePath, content, toEncoding);
}

QString EncodingManager::detectLineEnding(const QString &filePath) const
{
    // Delegate CRLF/CR/LF counting to the Rust backend.
    QByteArray pathBytes = filePath.toUtf8();
    char *result = rust_encoding_detect_line_ending(pathBytes.constData());
    if (!result) return "LF";
    QString le = QString::fromUtf8(result);
    rust_free_string(result);
    return le;
}

QString EncodingManager::convertLineEndings(const QString &content, const QString &fromStyle, const QString &toStyle)
{
    QByteArray contentBytes = content.toUtf8();
    QByteArray fromBytes = fromStyle.toUtf8();
    QByteArray toBytes = toStyle.toUtf8();
    char *result = rust_encoding_convert_line_endings(contentBytes.constData(),
                                                      fromBytes.constData(),
                                                      toBytes.constData());
    if (!result) return content;
    QString out = QString::fromUtf8(result);
    rust_free_string(result);
    return out;
}

bool EncodingManager::hasBOM(const QString &filePath) const
{
    QByteArray pathBytes = filePath.toUtf8();
    return rust_encoding_has_bom(pathBytes.constData());
}
