#ifndef ENCODINGMANAGER_H
#define ENCODINGMANAGER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QStringConverter>

/**
 * Manages file encoding detection and conversion.
 * Supports UTF-8, UTF-16, Latin-1, and other encodings.
 */
class EncodingManager : public QObject
{
    Q_OBJECT
public:
    explicit EncodingManager(QObject *parent = nullptr);

    // Detect encoding of a file
    QString detectEncoding(const QString &filePath) const;

    // Get all supported encodings
    QStringList supportedEncodings() const;

    // Get encoding display name
    QString encodingDisplayName(const QString &encoding) const;

    // Convert file content between encodings
    bool convertEncoding(const QString &filePath, const QString &fromEncoding, const QString &toEncoding);

    // Read file with specific encoding
    QString readFileWithEncoding(const QString &filePath, const QString &encoding) const;

    // Write file with specific encoding
    bool writeFileWithEncoding(const QString &filePath, const QString &content, const QString &encoding);

    // Detect line ending style (CRLF, LF, CR)
    QString detectLineEnding(const QString &filePath) const;

    // Convert line endings
    QString convertLineEndings(const QString &content, const QString &fromStyle, const QString &toStyle);

    // Check if BOM is present
    bool hasBOM(const QString &filePath) const;

signals:
    void encodingDetected(const QString &filePath, const QString &encoding);
    void lineEndingDetected(const QString &filePath, const QString &lineEnding);

private:
    struct EncodingInfo {
        QString name;
        QString displayName;
        bool isUnicode;
    };

    QMap<QString, EncodingInfo> m_encodings;
    void initEncodings();
};

#endif // ENCODINGMANAGER_H
