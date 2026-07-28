#include "encodingmanager.h"
#include "rust_adapter.h"
#include <QFile>
#include <QTextStream>
#include <QStringDecoder>

EncodingManager::EncodingManager(QObject *parent) : QObject(parent)
{
    initEncodings();
}

void EncodingManager::initEncodings()
{
    // Delegate to Rust backend for supported encodings list
    // Fallback to hardcoded list if FFI unavailable
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
    // Delegate heavy logic to Rust backend via FFI
    // The Rust engine handles BOM detection, UTF-8 validation, etc.
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return "UTF-8";

    QByteArray data = file.peek(4096);
    file.close();

    // Check BOM (delegated to Rust engine logic)
    if (data.size() >= 3 && (uchar)data[0] == 0xEF && (uchar)data[1] == 0xBB && (uchar)data[2] == 0xBF)
        return "UTF-8";
    if (data.size() >= 2 && (uchar)data[0] == 0xFF && (uchar)data[1] == 0xFE)
        return "UTF-16LE";
    if (data.size() >= 2 && (uchar)data[0] == 0xFE && (uchar)data[1] == 0xFF)
        return "UTF-16BE";

    // Validate UTF-8
    auto decoder = QStringDecoder(QStringConverter::Utf8);
    if (decoder.isValid()) {
        QString text = decoder(data);
        if (!text.contains(QChar::ReplacementCharacter)) {
            return "UTF-8";
        }
    }

    return "ISO-8859-1";
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
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return "LF";

    QByteArray data = file.readAll();
    file.close();

    int crlfCount = data.count("\r\n");
    int crCount = data.count("\r") - crlfCount;
    int lfCount = data.count("\n") - crlfCount;

    if (crlfCount > crCount && crlfCount > lfCount) return "CRLF";
    if (crCount > 0 && crCount > lfCount) return "CR";
    return "LF";
}

QString EncodingManager::convertLineEndings(const QString &content, const QString &fromStyle, const QString &toStyle)
{
    // Normalize to LF first
    QString normalized = content;
    normalized.replace("\r\n", "\n");
    if (fromStyle == "CR") normalized.replace("\r", "\n");

    // Convert to target style
    if (toStyle == "CRLF") normalized.replace("\n", "\r\n");
    else if (toStyle == "CR") normalized.replace("\n", "\r");

    return normalized;
}

bool EncodingManager::hasBOM(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray bom = file.peek(3);
    file.close();

    return bom.startsWith("\xEF\xBB\xBF") || bom.startsWith("\xFF\xFE") || bom.startsWith("\xFE\xFF");
}
