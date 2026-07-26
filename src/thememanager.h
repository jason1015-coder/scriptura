#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QPalette>
#include <QColor>
#include <QMap>
#include <QStringList>

// ── Explicit theme colour definition ──────────────────────────────────
// Each theme fully specifies every colour, so themes are self-contained
// and can be serialised, customised, or replaced without magic constants.
struct ThemeDefinition
{
    // ── Core UI colours ───────────────────────────────────────────
    QColor bgColor;           // Window / background
    QColor textColor;         // Normal text
    QColor svgColor;          // SVG icon fill (may differ from text)
    QColor buttonColor;       // Button face
    QColor highlightColor;    // Selection / accent

    // ── Derived / fine-grained UI colours ─────────────────────────
    QColor baseColor;         // Text-editor / input background
    QColor borderColor;       // Borders and separators
    QColor lightColor;        // Lighter shade for hover
    QColor midColor;          // Mid shade for disabled / borders
    QColor darkColor;         // Dark shade for shadows
    QColor buttonTextColor;   // Text on buttons
    QColor highlightedText;   // Text on highlighted / selected

    // ── Syntax highlighting colours ───────────────────────────────
    QColor synKeyword;
    QColor synString;
    QColor synComment;
    QColor synNumber;
    QColor synPreprocessor;
    QColor synTag;
    QColor synAttribute;
    QColor synCssProperty;
    QColor synVariable;
    QColor synFunction;
    QColor synEscape;

    // Convenience: pack syntax colours into a list matching
    // CodeHighlighter::setThemeColors parameter order.
    QList<QColor> syntaxColorList() const
    {
        return { synKeyword,   synString,    synComment,    synNumber,
                 synPreprocessor, synTag,    synAttribute,  synCssProperty,
                 synVariable, synFunction,  synEscape };
    }
};

class ThemeManager : public QObject
{
    Q_OBJECT
public:
    enum class ColorFamily {
        Default = 0,
        Blue = 1,
        Green = 2,
        Red = 3,
        Yellow = 4,
        Brown = 5,
        Cyan = 6,
        Violet = 7
    };

    enum class Mode {
        Light = 0,
        Dark = 1
    };

    enum class Feature {
        None = 0x0,
        HighContrast = 0x1
    };
    Q_DECLARE_FLAGS(Features, Feature)

    struct Theme {
        ColorFamily family;
        Mode mode;
        Features features;

        Theme(ColorFamily f = ColorFamily::Default, Mode m = Mode::Light, Features feat = Features())
            : family(f), mode(m), features(feat) {}

        bool isDark() const { return mode == Mode::Dark; }
        bool operator==(const Theme &other) const {
            return family == other.family && mode == other.mode && features == other.features;
        }
        bool operator!=(const Theme &other) const { return !(*this == other); }
    };

    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager();

    // Theme application
    void applyTheme(const Theme &theme);
    Theme currentTheme() const { return m_currentTheme; }
    void setCurrentTheme(const Theme &theme);

    // ── New: access the fully resolved colour definition ──────────
    ThemeDefinition currentDefinition() const { return m_currentDefinition; }

    // Build a definition for any family/mode combination (no side effects)
    static ThemeDefinition buildDefinition(ColorFamily family, Mode mode);

    // Build a QPalette from a definition (for Qt widget styling)
    static QPalette buildPalette(const ThemeDefinition &def);

    // Legacy helpers (internally use buildDefinition + buildPalette)
    QPalette buildBasePalette(ColorFamily family, Mode mode) const;

    // Design-token / stylesheet helpers
    QColor accentColor() const;
    QColor backgroundColor() const;
    QColor svgColor() const;
    QColor textColor() const;
    QColor borderColor() const;
    QStringList fontStack() const;
    QStringList monoFontStack() const;

    QString generateDesignTokens() const;
    QString generateGlobalStylesheet() const;

signals:
    void themeChanged(const Theme &theme);

private:
    Theme m_currentTheme;
    ThemeDefinition m_currentDefinition;   // cached resolved definition
    QMap<ColorFamily, QMap<Mode, QColor>> m_backgroundCache;
    QMap<ColorFamily, QMap<Mode, QColor>> m_accentCache;

    void buildCaches();
    QColor familyColor(ColorFamily family, Mode mode, int alpha = 255) const;
};

#endif // THEMEMANAGER_H
