#ifndef SRC_PLUGINEDITORAPI_H
#define SRC_PLUGINEDITORAPI_H

#include <QObject>
#include <QColor>
#include <QIcon>
#include <QString>
#include <QHash>
#include <QList>
#include <QPair>
#include <QTextEdit>

class MainWindow;

class PluginEditorApi : public QObject
{
    Q_OBJECT

public:
    explicit PluginEditorApi(MainWindow *mainWindow, QObject *parent = nullptr);
    ~PluginEditorApi() override;

    // Editor Decorations
    void addLineDecoration(const QString &id, int line, const QColor &color,
                           const QString &tooltip = {});
    void removeDecoration(const QString &id);
    void clearAllDecorations();

    // Gutter Markers
    void addGutterMarker(const QString &id, int line, const QIcon &icon,
                         const QString &tooltip = {});
    void removeGutterMarker(const QString &id, int line);
    void clearAllGutterMarkers();

    // Line Annotations
    void addAnnotation(const QString &id, int line, const QString &text,
                       const QColor &color = {});
    void removeAnnotation(const QString &id);
    void clearAllAnnotations();

    // Editor read access
    QString selectedText() const;
    int cursorLine() const;
    int cursorColumn() const;
    int lineCount() const;

    // Called by MainWindow when editor switches — reapplies decorations
    void onEditorChanged();

private:
    struct DecorationEntry {
        int line;
        QColor color;
        QString tooltip;
    };
    struct MarkerEntry {
        int line;
        QIcon icon;
        QString tooltip;
    };
    struct AnnotationEntry {
        int line;
        QString text;
        QColor color;
    };

    void applyDecorations();
    void applyMarkers();
    void applyAnnotations();
    QTextEdit::ExtraSelection makeSelection(int line, const QColor &color) const;

    MainWindow *m_mainWindow;
    QHash<QString, DecorationEntry> m_decorations;
    QHash<QString, MarkerEntry> m_markers;
    QHash<QString, AnnotationEntry> m_annotations;
};

#endif // SRC_PLUGINEDITORAPI_H
