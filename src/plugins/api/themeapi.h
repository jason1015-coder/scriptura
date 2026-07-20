#ifndef SRC_PLUGINTHEMEAPI_H
#define SRC_PLUGINTHEMEAPI_H

#include <QObject>
#include <QColor>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <functional>

class MainWindow;

/**
 * @file themeapi.h
 * @brief Plugin API for custom theme creation and management
 *
 * This API allows plugins to:
 * - Register custom color themes with full color palette control
 * - Define theme variants (light/dark) for each theme
 * - Register custom syntax highlighting colors
 * - Subscribe to theme change events
 * - Query current theme properties
 */
class PluginThemeApi : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Complete color palette for a theme
     *
     * Defines all colors needed for a complete UI theme.
     * Plugins can provide partial palettes - missing colors will use defaults.
     */
    struct ThemeColors {
        // Window colors
        QColor window;              ///< Main window background
        QColor windowText;          ///< Text on window background
        QColor base;                ///< Base background (editors, lists)
        QColor alternateBase;       ///< Alternating row background
        QColor text;                ///< Primary text color
        QColor placeholderText;     ///< Placeholder text color

        // Button colors
        QColor button;              ///< Button background
        QColor buttonText;          ///< Button text color

        // Highlight/Selection
        QColor highlight;           ///< Selection highlight
        QColor highlightedText;     ///< Text on highlight

        // Accent
        QColor accent;              ///< Primary accent color (links, active elements)
        QColor accentHover;         ///< Hover state for accent elements
        QColor accentPressed;       ///< Pressed state for accent elements

        // Borders
        QColor border;              ///< Default border color
        QColor mid;                 ///< Mid-tone border (less prominent)

        // States
        QColor success;             ///< Success state color
        QColor warning;             ///< Warning state color
        QColor error;               ///< Error state color
        QColor info;                ///< Info state color

        // Shadow/Depth
        QColor shadow;              ///< Shadow color
        QColor light;               ///< Lighter shade for hover states
        QColor midlight;            ///< Slightly lighter shade
        QColor dark;                ///< Darker shade
        QColor brightText;          ///< Bright text (e.g., for error messages)

        // Link colors
        QColor link;                ///< Link color
        QColor linkVisited;         ///< Visited link color

        // Diff/Git colors (optional)
        QColor diffAdded;           ///< Added line/file color
        QColor diffModified;        ///< Modified line/file color
        QColor diffDeleted;         ///< Deleted line/file color
        QColor diffConflict;        ///< Conflict color

        bool isDark() const { return window.lightness() < 128; }
        bool isValid() const { return window.isValid() || base.isValid(); }
    };

    /**
     * @brief Syntax highlighting colors for code editors
     */
    struct SyntaxColors {
        QColor keyword;             ///< Keywords (if, else, class, etc.)
        QColor string;              ///< String literals
        QColor comment;             ///< Comments
        QColor number;              ///< Numeric literals
        QColor preprocessor;        ///< Preprocessor directives
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
     * @brief Theme definition provided by a plugin
     */
    struct ThemeDefinition {
        QString id;                 ///< Unique theme identifier
        QString displayName;        ///< Human-readable name
        QString author;             ///< Theme author
        QString description;        ///< Theme description
        bool isDark = false;          ///< Whether this is a dark theme

        ThemeColors colors;         ///< Color palette
        SyntaxColors syntaxColors;  ///< Syntax highlighting colors

        // Optional: Custom stylesheet additions
        QString customStylesheet;   ///< Additional CSS/QSS rules

        // Font preferences (optional)
        QString fontFamily;         ///< Preferred font family
        QString monoFontFamily;     ///< Preferred monospace font family

        bool isValid() const { return !id.isEmpty() && !displayName.isEmpty(); }
    };

    explicit PluginThemeApi(MainWindow *mainWindow, QObject *parent = nullptr);
    ~PluginThemeApi() override;

    // ── Theme Registration ───────────────────────────────────────

    /**
     * @brief Register a custom theme from a plugin
     * @param definition Complete theme definition
     * @return true if theme was registered successfully
     *
     * The theme will appear in the theme selector and can be applied
     * by users through the settings UI.
     */
    bool registerTheme(const ThemeDefinition &definition);

    /**
     * @brief Unregister a previously registered theme
     * @param themeId The ID of the theme to unregister
     * @return true if theme was found and removed
     *
     * If the active theme is being unregistered, the app will
     * fall back to the default theme.
     */
    bool unregisterTheme(const QString &themeId);

    /**
     * @brief Check if a theme is registered
     * @param themeId Theme identifier to check
     * @return true if theme is registered
     */
    bool isThemeRegistered(const QString &themeId) const;

    /**
     * @brief Get the total number of registered plugin themes
     */
    int themeCount() const;

    // ── Theme Application ────────────────────────────────────────

    /**
     * @brief Apply a registered theme by ID
     * @param themeId ID of the theme to apply
     * @return true if theme was found and applied
     */
    bool applyTheme(const QString &themeId);

    /**
     * @brief Apply a custom theme definition without registering
     * @param definition Theme to apply temporarily
     *
     * This applies the theme but doesn't add it to the selector.
     * Useful for preview or temporary theme changes.
     */
    void applyThemeDefinition(const ThemeDefinition &definition);

    // ── Theme Query ──────────────────────────────────────────────

    /**
     * @brief Get the currently active theme ID
     * @return Theme ID, or empty string if using built-in theme
     */
    QString currentThemeId() const;

    /**
     * @brief Get a registered theme definition
     * @param themeId Theme identifier
     * @return Pointer to theme definition, or nullptr if not found
     */
    const ThemeDefinition* getThemeDefinition(const QString &themeId) const;

    /**
     * @brief List all registered theme IDs
     * @return List of theme IDs registered by plugins
     */
    QStringList registeredThemeIds() const;

    /**
     * @brief Get theme info for display purposes
     */
    struct ThemeInfo {
        QString id;
        QString displayName;
        QString author;
        QString description;
        bool isDark;
    };

    /**
     * @brief Get info about all registered themes
     */
    QList<ThemeInfo> availableThemes() const;

    // ── Color Utilities ──────────────────────────────────────────

    /**
     * @brief Get a color from the current theme by semantic name
     * @param colorName Semantic color name (e.g., "accent", "error", "success")
     * @return The color, or invalid QColor if not found
     */
    QColor getColorByName(const QString &colorName) const;

    /**
     * @brief Generate CSS custom properties for the current theme
     * @return CSS string with custom properties
     *
     * Plugins can use this to style their widgets to match the theme.
     * Example output:
     *   --theme-accent: #007AFF;
     *   --theme-bg: #1E1E1E;
     */
    QString generateCSSCustomProperties() const;

    // ── Syntax Colors ────────────────────────────────────────────

    /**
     * @brief Get syntax colors for the current theme
     * @return SyntaxColors for the active theme
     */
    SyntaxColors currentSyntaxColors() const;

    /**
     * @brief Override syntax colors for the current theme
     * @param colors Custom syntax colors to apply
     *
     * This allows plugins to customize syntax highlighting
     * without replacing the entire theme.
     */
    void setSyntaxColorOverride(const SyntaxColors &colors);

    /**
     * @brief Clear syntax color overrides
     */
    void clearSyntaxColorOverrides();

    // ── Signals ──────────────────────────────────────────────────

signals:
    /**
     * @brief Emitted when a theme is registered
     * @param themeId ID of the registered theme
     */
    void themeRegistered(const QString &themeId);

    /**
     * @brief Emitted when a theme is unregistered
     * @param themeId ID of the unregistered theme
     */
    void themeUnregistered(const QString &themeId);

    /**
     * @brief Emitted when the active theme changes
     * @param themeId ID of the new theme (empty for built-in)
     */
    void activeThemeChanged(const QString &themeId);

    /**
     * @brief Emitted when theme colors are needed for rendering
     * @param themeId The theme being rendered
     */
    void themeRendering(const QString &themeId);

private:
    MainWindow *m_mainWindow;
    QMap<QString, ThemeDefinition> m_themes;
    QString m_currentThemeId;
    bool m_syntaxOverrideActive = false;
    SyntaxColors m_syntaxOverride;
};

// Make ThemeDefinition available in QVariant
Q_DECLARE_METATYPE(PluginThemeApi::ThemeDefinition)
Q_DECLARE_METATYPE(PluginThemeApi::ThemeColors)
Q_DECLARE_METATYPE(PluginThemeApi::SyntaxColors)

#endif // SRC_PLUGINTHEMEAPI_H
