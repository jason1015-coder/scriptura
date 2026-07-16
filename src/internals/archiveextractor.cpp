#include "archiveextractor.h"

#include <zlib.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace Scriptura {
namespace {

// ---- Minimal ZIP reader (store + deflate) using zlib ----
// Enough to extract plugin archives without any external tool or Qt private API.

bool readBytes(QFile &f, qint64 pos, qint64 size, QByteArray &out)
{
    if (size < 0 || !f.seek(pos))
        return false;
    out = f.read(size);
    return out.size() == static_cast<int>(size);
}

quint32 le32(const char *p)
{
    return quint32(quint8(p[0])) | (quint32(quint8(p[1])) << 8) |
           (quint32(quint8(p[2])) << 16) | (quint32(quint8(p[3])) << 24);
}

quint16 le16(const char *p)
{
    return quint16(quint8(p[0])) | (quint16(quint8(p[1])) << 8);
}

bool findEndRecord(QFile &f, quint32 &cdOffset, quint16 &totalEntries)
{
    const qint64 fileSize = f.size();
    if (fileSize < 22) return false;
    const qint64 back = qMin(qint64(65557), fileSize);
    QByteArray buf;
    if (!readBytes(f, fileSize - back, back, buf))
        return false;
    for (qint64 i = buf.size() - 22; i >= 0; --i) {
        if (le32(buf.constData() + i) == 0x06054b50) {
            const char *p = buf.constData() + i;
            totalEntries = le16(p + 10);
            cdOffset = le32(p + 16);
            return true;
        }
    }
    return false;
}

bool inflateDeflate(const QByteArray &in, QByteArray &out)
{
    z_stream strm{};
    strm.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
    strm.avail_in = static_cast<uInt>(in.size());

    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK)
        return false;

    out.clear();
    char chunk[65536];
    int ret = Z_OK;
    do {
        strm.next_out = reinterpret_cast<Bytef *>(chunk);
        strm.avail_out = sizeof(chunk);
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR)
            break;
        out.append(chunk, sizeof(chunk) - strm.avail_out);
    } while (ret != Z_STREAM_END && strm.avail_out == 0);

    inflateEnd(&strm);
    return ret == Z_STREAM_END || ret == Z_OK;
}

} // namespace

bool ArchiveExtractor::extract(const QString &archivePath,
                               const QString &destDir,
                               QString *error)
{
    if (archivePath.isEmpty() || destDir.isEmpty()) {
        if (error) *error = "ArchiveExtractor: empty path argument";
        return false;
    }

    QFile f(archivePath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = "ArchiveExtractor: cannot open archive: " + archivePath;
        return false;
    }

    quint32 cdOffset = 0;
    quint16 totalEntries = 0;
    if (!findEndRecord(f, cdOffset, totalEntries)) {
        if (error) *error = "ArchiveExtractor: not a valid zip archive";
        return false;
    }

    QDir dest(destDir);
    if (!dest.exists() && !dest.mkpath(".")) {
        if (error) *error = "ArchiveExtractor: cannot create destination: " + destDir;
        return false;
    }
    const QString destAbsolute = dest.absolutePath();

    qint64 pos = cdOffset;
    for (quint16 i = 0; i < totalEntries; ++i) {
        char hdr[46];
        QByteArray hdrBuf;
        if (!readBytes(f, pos, 46, hdrBuf) || hdrBuf.size() != 46) {
            if (error) *error = "ArchiveExtractor: corrupt central directory";
            return false;
        }
        memcpy(hdr, hdrBuf.constData(), 46);
        if (le32(hdr) != 0x02014b50) {
            if (error) *error = "ArchiveExtractor: bad central-directory signature";
            return false;
        }

        quint16 method = le16(hdr + 10);
        quint32 compSize = le32(hdr + 20);
        quint32 uncompSize = le32(hdr + 24);
        quint16 nameLen = le16(hdr + 28);
        quint16 extraLen = le16(hdr + 30);
        quint16 commentLen = le16(hdr + 32);
        quint32 localOffset = le32(hdr + 42);

        QByteArray nameBuf;
        if (!readBytes(f, pos + 46, nameLen, nameBuf)) {
            if (error) *error = "ArchiveExtractor: corrupt entry name";
            return false;
        }
        QString entryName = QString::fromUtf8(nameBuf);

        // Reject absolute paths and zip-slip traversal entries.
        QString clean = QDir::cleanPath(entryName);
        if (clean.startsWith("..") || clean == ".." || QDir::isAbsolutePath(clean)) {
            if (error) *error = "ArchiveExtractor: rejected unsafe entry: " + entryName;
            return false;
        }
        QString absTarget = QDir(destAbsolute).absoluteFilePath(clean);
        if (absTarget != destAbsolute &&
            !absTarget.startsWith(destAbsolute + QDir::separator())) {
            if (error) *error = "ArchiveExtractor: rejected escaped entry: " + entryName;
            return false;
        }

        if (entryName.endsWith('/')) {
            QDir().mkpath(absTarget);
            pos += 46 + nameLen + extraLen + commentLen;
            continue;
        }

        char lhdr[30];
        QByteArray lhdrBuf;
        if (!readBytes(f, localOffset, 30, lhdrBuf) || lhdrBuf.size() != 30) {
            if (error) *error = "ArchiveExtractor: corrupt local header";
            return false;
        }
        memcpy(lhdr, lhdrBuf.constData(), 30);
        if (le32(lhdr) != 0x04034b50) {
            if (error) *error = "ArchiveExtractor: bad local header signature";
            return false;
        }
        quint16 lNameLen = le16(lhdr + 26);
        quint16 lExtraLen = le16(lhdr + 28);
        qint64 dataPos = localOffset + 30 + lNameLen + lExtraLen;

        QByteArray comp;
        if (!readBytes(f, dataPos, compSize, comp) ||
            comp.size() != static_cast<int>(compSize)) {
            if (error) *error = "ArchiveExtractor: truncated entry: " + entryName;
            return false;
        }

        QByteArray outData;
        if (method == 0) {            // stored
            outData = comp;
        } else if (method == 8) {     // deflate
            if (!inflateDeflate(comp, outData)) {
                if (error) *error = "ArchiveExtractor: inflate failed: " + entryName;
                return false;
            }
        } else {
            if (error) *error = QString("ArchiveExtractor: unsupported method %1: %2")
                                    .arg(method).arg(entryName);
            return false;
        }

        if (outData.size() != static_cast<int>(uncompSize)) {
            if (error) *error = "ArchiveExtractor: size mismatch: " + entryName;
            return false;
        }

        QDir().mkpath(QFileInfo(absTarget).absolutePath());
        QSaveFile out(absTarget);
        if (!out.open(QIODevice::WriteOnly) || out.write(outData) == -1 || !out.commit()) {
            if (error) *error = "ArchiveExtractor: write failed: " + absTarget;
            return false;
        }

        pos += 46 + nameLen + extraLen + commentLen;
    }

    return true;
}

} // namespace Scriptura
