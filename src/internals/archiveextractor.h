#ifndef ARCHIVEEXTRACTOR_H
#define ARCHIVEEXTRACTOR_H

#include <QString>

namespace Scriptura {
/**
 * @brief Cross-platform ZIP extraction utilities.
 *
 * Replaces the previous reliance on the external `unzip` binary, which is
 * unavailable on Windows and created a silent plugin-install failure there.
 * Extraction is performed in-process via Qt's QZipReader and is hardened
 * against path-traversal ("zip-slip") attacks.
 */
class ArchiveExtractor
{
public:
    /**
     * @brief Extract a ZIP archive into @p destDir.
     * @param archivePath Absolute path to the .zip file.
     * @param destDir Destination directory (created if missing).
     * @param error Optional output: human-readable error when false is returned.
     * @return true on success, false on any failure (missing archive,
     *         corrupt zip, or a rejected path-traversal entry).
     */
    static bool extract(const QString &archivePath,
                        const QString &destDir,
                        QString *error = nullptr);
};
}

#endif // ARCHIVEEXTRACTOR_H
