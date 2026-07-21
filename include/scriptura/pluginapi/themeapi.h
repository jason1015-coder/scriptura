#ifndef SCRIPTURA_PLUGINAPI_THEMEAPI_H
#define SCRIPTURA_PLUGINAPI_THEMEAPI_H

/**
 * @file themeapi.h
 * @brief SDK header for Scriptura Theme Plugin API
 *
 * This header provides the API for plugins that want to create custom themes.
 * Include this header in your theme plugin to:
 *   - Define custom color palettes
 *   - Register themes with the application
 *   - Respond to theme change events
 *
 * Example usage:
 * @code
 *   #include <scriptura/pluginapi/themeapi.h>
 *
 *   class MyThemePlugin : public ScripturaPlugin {
 *       bool initialize(PluginContext* context) override {
 *           if (auto* theme = context->theme()) {
 *               PluginThemeApi::ThemeDefinition def;
 *               def.id = "com.example.mytheme";
 *               def.displayName = "My Custom Theme";
 *               def.author = "Me";
 *               def.isDark = true;
 *
 *               def.colors.window = QColor(30, 30, 40);
 *               def.colors.accent = QColor(100, 200, 255);
 *               def.colors.text = QColor(220, 220, 230);
 *               // ... more colors
 *
 *               theme->registerTheme(def);
 *               theme->applyTheme(def.id);
 *           }
 *           return true;
 *       }
 *   };
 * @endcode
 */

#include <QColor>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QObject>

// Forward declaration - the actual class is defined in the host application
class PluginThemeApi;

/**
 * @brief Color palette definition for a custom theme
 *
 * Define all colors needed for your theme. Only the colors you set
 * will be applied - others will use sensible defaults.
 */
struct ScripturaThemeColors {
    // ── Core Colors ──────────────────────────────────────────────
    QColor window;              ///< Main window background
    QColor windowText;          ///< Text on window background
    QColor base;                ///< Base background (editors, lists)
    QColor alternateBase;       ///< Alternating row background
    QColor text;                ///< Primary text color
    QColor placeholderText;     ///< Placeholder text color

    // ── Button Colors ────────────────────────────────────────────
    QColor button;              ///< Button background
    QColor buttonText;          ///< Button text color

    // ── Selection ────────────────────────────────────────────────
    QColor highlight;           ///< Selection highlight
    QColor highlightedText;     ///< Text on highlight

    // ── Accent Colors ────────────────────────────────────────────
    QColor accent;              ///< Primary accent (links, active elements)
    QColor accentHover;         ///< Hover state for accent elements
    QColor accentPressed;       ///< Pressed state for accent elements

    // ── Border Colors ────────────────────────────────────────────
    QColor border;              ///< Default border color
    QColor mid;                 ///< Mid-tone border (less prominent)

    // ── State Colors ─────────────────────────────────────────────
    QColor success;             ///< Success state (green)
    QColor warning;             ///< Warning state (yellow/orange)
    QColor error;               ///< Error state (red)
    QColor info;                ///< Info state (blue)

    // ── Depth/Shadow ─────────────────────────────────────────────
    QColor shadow;              ///< Shadow color
    QColor light;               ///< Lighter shade for hover
    QColor dark;                ///< Darker shade

    // ── Link Colors ──────────────────────────────────────────────
    QColor link;                ///< Link color
    QColor linkVisited;         ///< Visited link color

    // ── Diff/Git Colors ──────────────────────────────────────────
    QColor diffAdded;           ///< Added line/file
    QColor diffModified;        ///< Modified line/file
    QColor diffDeleted;         ///< Deleted line/file
    QColor diffConflict;        ///< Conflict marker
};

/**
 * @brief Syntax highlighting colors for code editors
 *
 * Define colors for different syntax elements in the code editor.
 */
struct ScripturaSyntaxColors {
    QColor keyword;             ///< Keywords (if, else, class, etc.)
    QColor string;              ///< String literals
    QColor comment;             ///< Comments
    QColor number;              ///< Numeric literals
    QColor preprocessor;        ///< Preprocessor directives (#include, etc.)
    QColor tag;                 ///< HTML/XML tags
    QColor attribute;           ///< HTML/XML attributes
    QColor cssProperty;         ///< CSS properties
    QColor variable;            ///< Variables
    QColor function;            ///< Function names
    QColor escape;              ///< Escape sequences
    QColor type;                ///< Type names
    QColor operator_;           ///< Operators
    QColor punctuation;         ///< Punctuation
};

/**
 * @brief Complete theme definition
 *
 * Bundle all theme information into a single structure.
 */
struct ScripturaThemeDefinition {
    QString id;                 ///< Unique theme ID (e.g., "com.example.mytheme")
    QString displayName;        ///< Human-readable name
    QString author;             ///< Theme author
    QString description;        ///< Theme description
    bool isDark = true;         ///< Whether this is a dark theme

    ScripturaThemeColors colors;       ///< Color palette
    ScripturaSyntaxColors syntaxColors; ///< Syntax highlighting colors

    // Optional
    QString customStylesheet;   ///< Additional QSS rules
    QString fontFamily;         ///< Preferred UI font
    QString monoFontFamily;     ///< Preferred monospace font

    bool isValid() const { return !id.isEmpty() && !displayName.isEmpty(); }
};

// Default dark theme colors
inline ScripturaThemeColors scripturaDefaultDarkColors() {
    ScripturaThemeColors c;
    c.window       = QColor(45, 45, 45);
    c.windowText   = QColor(220, 220, 220);
    c.base         = QColor(30, 30, 30);
    c.alternateBase = QColor(45, 45, 45);
    c.text         = QColor(220, 220, 220);
    c.placeholderText = QColor(140, 140, 140);
    c.button       = QColor(45, 45, 45);
    c.buttonText   = QColor(220, 220, 220);
    c.highlight    = QColor(0, 122, 255);
    c.highlightedText = QColor(255, 255, 255);
    c.accent       = QColor(0, 122, 255);
    c.accentHover  = QColor(40, 150, 255);
    c.accentPressed = QColor(0, 100, 220);
    c.border       = QColor(80, 80, 80);
    c.mid          = QColor(60, 60, 60);
    c.success      = QColor(80, 180, 80);
    c.warning      = QColor(255, 175, 56);
    c.error        = QColor(220, 50, 47);
    c.info         = QColor(70, 130, 200);
    c.shadow       = QColor(0, 0, 0);
    c.light        = QColor(70, 70, 70);
    c.dark         = QColor(20, 20, 20);
    c.link         = QColor(0, 122, 255);
    c.linkVisited  = QColor(128, 0, 128);
    c.diffAdded    = QColor(80, 180, 80);
    c.diffModified = QColor(200, 200, 80);
    c.diffDeleted  = QColor(220, 50, 47);
    c.diffConflict = QColor(220, 100, 47);
    return c;
}

// Default dark syntax colors
inline ScripturaSyntaxColors scripturaDefaultDarkSyntaxColors() {
    ScripturaSyntaxColors c;
    c.keyword      = QColor(220, 120, 255);  // Purple
    c.string       = QColor(150, 210, 100);  // Green
    c.comment      = QColor(120, 120, 120);  // Gray
    c.number       = QColor(180, 130, 60);   // Orange
    c.preprocessor = QColor(200, 80, 80);    // Red
    c.tag          = QColor(80, 180, 240);   // Blue
    c.attribute    = QColor(200, 200, 100);  // Yellow
    c.cssProperty  = QColor(120, 200, 200);  // Cyan
    c.variable     = QColor(220, 180, 100);  // Yellow-orange
    c.function     = QColor(100, 180, 255);  // Light blue
    c.escape       = QColor(255, 150, 100);  // Peach
    c.type         = QColor(80, 200, 200);   // Teal
    c.operator_    = QColor(220, 220, 220);  // Light gray
    c.punctuation  = QColor(180, 180, 180);  // Medium gray
    return c;
}

// Default light theme colors
inline ScripturaThemeColors scripturaDefaultLightColors() {
    ScripturaThemeColors c;
    c.window       = QColor(255, 255, 255);
    c.windowText   = QColor(30, 30, 30);
    c.base         = QColor(255, 255, 255);
    c.alternateBase = QColor(245, 245, 245);
    c.text         = QColor(30, 30, 30);
    c.placeholderText = QColor(140, 140, 140);
    c.button       = QColor(240, 240, 240);
    c.buttonText   = QColor(30, 30, 30);
    c.highlight    = QColor(0, 122, 255);
    c.highlightedText = QColor(255, 255, 255);
    c.accent       = QColor(0, 122, 255);
    c.accentHover  = QColor(40, 150, 255);
    c.accentPressed = QColor(0, 100, 220);
    c.border       = QColor(200, 200, 200);
    c.mid          = QColor(180, 180, 180);
    c.success      = QColor(40, 140, 40);
    c.warning      = QColor(200, 140, 0);
    c.error        = QColor(200, 40, 37);
    c.info         = QColor(40, 100, 180);
    c.shadow       = QColor(150, 150, 150);
    c.light        = QColor(230, 230, 230);
    c.dark         = QColor(180, 180, 180);
    c.link         = QColor(0, 122, 255);
    c.linkVisited  = QColor(128, 0, 128);
    c.diffAdded    = QColor(40, 140, 40);
    c.diffModified = QColor(160, 160, 40);
    c.diffDeleted  = QColor(200, 40, 37);
    c.diffConflict = QColor(200, 80, 37);
    return c;
}

#endif // SCRIPTURA_PLUGINAPI_THEMEAPI_H
