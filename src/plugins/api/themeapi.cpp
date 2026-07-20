#include "themeapi.h"
#include "mainwindow.h"
#include <QDebug>
#include <QApplication>

PluginThemeApi::PluginThemeApi(MainWindow *mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
}

PluginThemeApi::~PluginThemeApi() = default;

// ── Theme Registration ───────────────────────────────────────────

bool PluginThemeApi::registerTheme(const ThemeDefinition &definition)
{
    if (!definition.isValid()) {
        qWarning() << "PluginThemeApi: Cannot register theme with empty id or displayName";
        return false;
    }

    if (m_themes.contains(definition.id)) {
        qWarning() << "PluginThemeApi: Theme already registered:" << definition.id;
        return false;
    }

    m_themes.insert(definition.id, definition);
    qDebug() << "PluginThemeApi: Theme registered:" << definition.displayName
             << "(" << definition.id << ") by" << definition.author;

    emit themeRegistered(definition.id);
    return true;
}

bool PluginThemeApi::unregisterTheme(const QString &themeId)
{
    if (!m_themes.contains(themeId))
        return false;

    m_themes.remove(themeId);

    if (m_currentThemeId == themeId) {
        m_currentThemeId.clear();
        emit activeThemeChanged(QString());
    }

    emit themeUnregistered(themeId);
    return true;
}

bool PluginThemeApi::isThemeRegistered(const QString &themeId) const
{
    return m_themes.contains(themeId);
}

int PluginThemeApi::themeCount() const
{
    return m_themes.size();
}

// ── Theme Application ────────────────────────────────────────────

bool PluginThemeApi::applyTheme(const QString &themeId)
{
    const ThemeDefinition *def = getThemeDefinition(themeId);
    if (!def) {
        qWarning() << "PluginThemeApi: Theme not found:" << themeId;
        return false;
    }

    applyThemeDefinition(*def);
    m_currentThemeId = themeId;

    emit activeThemeChanged(themeId);
    return true;
}

void PluginThemeApi::applyThemeDefinition(const ThemeDefinition &definition)
{
    if (!m_mainWindow)
        return;

    emit themeRendering(definition.id);

    // Build QPalette from theme colors
    QPalette palette;
    const ThemeColors &c = definition.colors;

    if (c.isValid()) {
        // Use provided colors
        palette.setColor(QPalette::Window, c.window.isValid() ? c.window : QColor(45, 45, 45));
        palette.setColor(QPalette::WindowText, c.windowText.isValid() ? c.windowText : QColor(220, 220, 220));
        palette.setColor(QPalette::Base, c.base.isValid() ? c.base : QColor(30, 30, 30));
        palette.setColor(QPalette::AlternateBase, c.alternateBase.isValid() ? c.alternateBase : QColor(45, 45, 45));
        palette.setColor(QPalette::Text, c.text.isValid() ? c.text : QColor(220, 220, 220));
        palette.setColor(QPalette::Button, c.button.isValid() ? c.button : QColor(45, 45, 45));
        palette.setColor(QPalette::ButtonText, c.buttonText.isValid() ? c.buttonText : QColor(220, 220, 220));
        palette.setColor(QPalette::Highlight, c.highlight.isValid() ? c.highlight : c.accent);
        palette.setColor(QPalette::HighlightedText, c.highlightedText.isValid() ? c.highlightedText : QColor(255, 255, 255));
        palette.setColor(QPalette::Link, c.link.isValid() ? c.link : c.accent);
        palette.setColor(QPalette::LinkVisited, c.linkVisited.isValid() ? c.linkVisited : QColor(128, 0, 128));
        palette.setColor(QPalette::Mid, c.mid.isValid() ? c.mid : QColor(80, 80, 80));
        palette.setColor(QPalette::Light, c.light.isValid() ? c.light : QColor(70, 70, 70));
        palette.setColor(QPalette::Midlight, c.midlight.isValid() ? c.midlight : QColor(60, 60, 60));
        palette.setColor(QPalette::Dark, c.dark.isValid() ? c.dark : QColor(20, 20, 20));
        palette.setColor(QPalette::Shadow, c.shadow.isValid() ? c.shadow : QColor(0, 0, 0));
        palette.setColor(QPalette::BrightText, c.brightText.isValid() ? c.brightText : Qt::red);
        if (c.placeholderText.isValid())
            palette.setColor(QPalette::PlaceholderText, c.placeholderText);
    } else {
        // Use ThemeManager defaults for dark/light
        if (definition.isDark) {
            palette.setColor(QPalette::Window, QColor(45, 45, 45));
            palette.setColor(QPalette::WindowText, QColor(220, 220, 220));
            palette.setColor(QPalette::Base, QColor(30, 30, 30));
            palette.setColor(QPalette::Text, QColor(220, 220, 220));
        } else {
            palette.setColor(QPalette::Window, QColor(255, 255, 255));
            palette.setColor(QPalette::WindowText, QColor(30, 30, 30));
            palette.setColor(QPalette::Base, QColor(255, 255, 255));
            palette.setColor(QPalette::Text, QColor(30, 30, 30));
        }
    }

    QApplication::setPalette(palette);

    // Apply custom stylesheet if provided
    // Note: we clear previous plugin styles to avoid accumulation
    if (!definition.customStylesheet.isEmpty()) {
        QApplication *app = qobject_cast<QApplication*>(QApplication::instance());
        if (app) {
            // Remove previous plugin theme stylesheet marker if present
            QString existingSheet = app->styleSheet();
            int markerStart = existingSheet.indexOf(QStringLiteral("/* Plugin Theme:"));
            if (markerStart >= 0) {
                existingSheet = existingSheet.left(markerStart);
            }
            existingSheet += QStringLiteral("\n/* Plugin Theme: %1 */\n%2")
                                .arg(definition.displayName, definition.customStylesheet);
            app->setStyleSheet(existingSheet);
        }
    }

    // Apply syntax color overrides if defined
    if (definition.syntaxColors.keyword.isValid()) {
        setSyntaxColorOverride(definition.syntaxColors);
    }
}

// ── Theme Query ──────────────────────────────────────────────────

QString PluginThemeApi::currentThemeId() const
{
    return m_currentThemeId;
}

const PluginThemeApi::ThemeDefinition* PluginThemeApi::getThemeDefinition(const QString &themeId) const
{
    auto it = m_themes.find(themeId);
    return (it != m_themes.end()) ? &(*it) : nullptr;
}

QStringList PluginThemeApi::registeredThemeIds() const
{
    return m_themes.keys();
}

QList<PluginThemeApi::ThemeInfo> PluginThemeApi::availableThemes() const
{
    QList<ThemeInfo> result;
    for (auto it = m_themes.constBegin(); it != m_themes.constEnd(); ++it) {
        ThemeInfo info;
        info.id = it.value().id;
        info.displayName = it.value().displayName;
        info.author = it.value().author;
        info.description = it.value().description;
        info.isDark = it.value().isDark;
        result.append(info);
    }
    return result;
}

// ── Color Utilities ──────────────────────────────────────────────

QColor PluginThemeApi::getColorByName(const QString &colorName) const
{
    if (m_currentThemeId.isEmpty()) {
        // Return default colors based on current palette
        QPalette pal = QApplication::palette();
        if (colorName == "accent") return pal.highlight().color();
        if (colorName == "background") return pal.window().color();
        if (colorName == "text") return pal.text().color();
        if (colorName == "border") return pal.mid().color();
        if (colorName == "error") return QColor(220, 50, 47);
        if (colorName == "warning") return QColor(255, 175, 56);
        if (colorName == "success") return QColor(80, 180, 80);
        if (colorName == "info") return QColor(70, 130, 200);
        return QColor();
    }

    const ThemeDefinition *def = getThemeDefinition(m_currentThemeId);
    if (!def)
        return QColor();

    const ThemeColors &c = def->colors;

    // Map semantic names to theme colors
    if (colorName == "accent")           return c.accent;
    if (colorName == "accentHover")      return c.accentHover;
    if (colorName == "accentPressed")    return c.accentPressed;
    if (colorName == "background")       return c.window;
    if (colorName == "surface")          return c.base;
    if (colorName == "text")             return c.text;
    if (colorName == "border")           return c.border;
    if (colorName == "mid")              return c.mid;
    if (colorName == "success")          return c.success;
    if (colorName == "warning")          return c.warning;
    if (colorName == "error")            return c.error;
    if (colorName == "info")             return c.info;
    if (colorName == "diffAdded")        return c.diffAdded;
    if (colorName == "diffModified")     return c.diffModified;
    if (colorName == "diffDeleted")      return c.diffDeleted;
    if (colorName == "diffConflict")     return c.diffConflict;

    return QColor();
}

QString PluginThemeApi::generateCSSCustomProperties() const
{
    QStringList props;
    props << ":root {";

    auto addProp = [&](const QString &name, const QColor &color) {
        if (color.isValid()) {
            props << QString("    --theme-%1: %2;").arg(name, color.name());
        }
    };

    if (m_currentThemeId.isEmpty()) {
        QPalette pal = QApplication::palette();
        addProp("accent", pal.highlight().color());
        addProp("background", pal.window().color());
        addProp("text", pal.text().color());
        addProp("border", pal.mid().color());
    } else {
        const ThemeDefinition *def = getThemeDefinition(m_currentThemeId);
        if (def) {
            const ThemeColors &c = def->colors;
            addProp("accent", c.accent);
            addProp("accent-hover", c.accentHover);
            addProp("accent-pressed", c.accentPressed);
            addProp("background", c.window);
            addProp("surface", c.base);
            addProp("text", c.text);
            addProp("border", c.border);
            addProp("mid", c.mid);
            addProp("success", c.success);
            addProp("warning", c.warning);
            addProp("error", c.error);
            addProp("info", c.info);
            addProp("link", c.link);
            addProp("link-visited", c.linkVisited);
            addProp("shadow", c.shadow);
            addProp("light", c.light);
            addProp("dark", c.dark);
            addProp("diff-added", c.diffAdded);
            addProp("diff-modified", c.diffModified);
            addProp("diff-deleted", c.diffDeleted);
            addProp("diff-conflict", c.diffConflict);
        }
    }

    props << "}";
    return props.join("\n");
}

// ── Syntax Colors ────────────────────────────────────────────────

PluginThemeApi::SyntaxColors PluginThemeApi::currentSyntaxColors() const
{
    if (m_syntaxOverrideActive)
        return m_syntaxOverride;

    if (!m_currentThemeId.isEmpty()) {
        const ThemeDefinition *def = getThemeDefinition(m_currentThemeId);
        if (def && def->syntaxColors.keyword.isValid())
            return def->syntaxColors;
    }

    // Return default dark syntax colors
    SyntaxColors defaults;
    defaults.keyword      = QColor(220, 120, 255);  // Purple
    defaults.string       = QColor(150, 210, 100);  // Green
    defaults.comment      = QColor(120, 120, 120);  // Gray
    defaults.number       = QColor(180, 130, 60);   // Orange
    defaults.preprocessor = QColor(200, 80, 80);    // Red
    defaults.tag          = QColor(80, 180, 240);   // Blue
    defaults.attribute    = QColor(200, 200, 100);  // Yellow
    defaults.cssProperty  = QColor(120, 200, 200);  // Cyan
    defaults.variable     = QColor(220, 180, 100);  // Yellow-orange
    defaults.function     = QColor(100, 180, 255);  // Light blue
    defaults.escape       = QColor(255, 150, 100);  // Peach
    defaults.type         = QColor(80, 200, 200);   // Teal
    defaults.operator_    = QColor(220, 220, 220);  // Light gray
    defaults.punctuation  = QColor(180, 180, 180);  // Medium gray

    return defaults;
}

void PluginThemeApi::setSyntaxColorOverride(const SyntaxColors &colors)
{
    m_syntaxOverride = colors;
    m_syntaxOverrideActive = true;
}

void PluginThemeApi::clearSyntaxColorOverrides()
{
    m_syntaxOverrideActive = false;
    m_syntaxOverride = SyntaxColors();
}
