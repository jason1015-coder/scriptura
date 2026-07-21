#ifndef PLUGINEDITORAPI_H
#define PLUGINEDITORAPI_H

#include <QObject>
#include <QColor>
#include <QIcon>
#include <QString>

class MainWindow;

/**
 * @file editorapi.h
 * @brief Plugin Editor API — decorations, annotations, gutter markers
 *
 * Lets plugins decorate the active editor with visual markers,
 * line annotations, and gutter icons. All coordinates are 0-based line numbers.
 */
class PluginEditorApi : public QObject
{
    Q_OBJECT

public:
    explicit PluginEditorApi(MainWindow *mainWindow, QObject *parent = nullptr);
    ~PluginEditorApi() override;

    // ── Editor Decorations ─────────────────────────────────────

    /**
     * @brief Highlight a line in the editor with a background colour.
     * @param id       Unique decoration id (replaces any existing with same id)
     * @param line     0-based line number
     * @param color    Background colour (transparent is ignored)
     * @param tooltip  Optional tooltip shown on hover
     */
    void addLineDecoration(const QString &id, int line, const QColor &color,
                           const QString &tooltip = {});

    /**
     * @brief Remove a line decoration.
     * @param id  Decoration id passed to addLineDecoration()
     */
    void removeDecoration(const QString &id);

    /**
     * @brief Remove all decorations added by this plugin.
     */
    void clearAllDecorations();

    // ── Gutter Markers ─────────────────────────────────────────

    /**
     * @brief Place an icon in the line-number gutter at a given line.
     * @param id       Unique marker id
     * @param line     0-based line number
     * @param icon     Icon to display
     * @param tooltip  Optional tooltip
     */
    void addGutterMarker(const QString &id, int line, const QIcon &icon,
                         const QString &tooltip = {});

    /**
     * @brief Remove a gutter marker.
     * @param id   Marker id
     * @param line Line number (ignored if marker id is unique across all lines)
     */
    void removeGutterMarker(const QString &id, int line);

    /**
     * @brief Remove all gutter markers.
     */
    void clearAllGutterMarkers();

    // ── Line Annotations ───────────────────────────────────────

    /**
     * @brief Show annotation text below a given line.
     * @param id    Unique annotation id
     * @param line  0-based line number
     * @param text  Text to display
     * @param color Optional text colour
     */
    void addAnnotation(const QString &id, int line, const QString &text,
                       const QColor &color = {});

    /**
     * @brief Remove an annotation.
     * @param id  Annotation id
     */
    void removeAnnotation(const QString &id);

    /**
     * @brief Remove all annotations.
     */
    void clearAllAnnotations();

    // ── Editor Access (read-only helpers, permission-gated) ────

    /**
     * @brief Get the text currently selected in the active editor.
     */
    QString selectedText() const;

    /**
     * @brief Get the 0-based line number of the cursor in the active editor.
     */
    int cursorLine() const;

    /**
     * @brief Get the 0-based column of the cursor in the active editor.
     */
    int cursorColumn() const;

    /**
     * @brief Get the total line count of the active document.
     */
    int lineCount() const;

private:
    MainWindow *m_mainWindow;
};

#endif // PLUGINEDITORAPI_H
