#ifndef THEMEDEFS_H
#define THEMEDEFS_H

#include <QFlags>

/**
 * Lightweight header for theme enum definitions.
 * Separated from mainwindow.h to avoid heavy transitive includes.
 */

enum class ThemeColorFamily {
    Default = 0,
    Blue = 1,
    Green = 2,
    Red = 3,
    Yellow = 4,
    Brown = 5,
    Cyan = 6,
    Violet = 7
};

enum class ThemeMode {
    Light = 0,
    Dark = 1
};

enum class ThemeFeature {
    None = 0x0,
    HighContrast = 0x1
};

Q_DECLARE_FLAGS(ThemeFeatures, ThemeFeature)

struct Theme {
    ThemeColorFamily family;
    ThemeMode mode;
    ThemeFeatures features;

    Theme(ThemeColorFamily f = ThemeColorFamily::Default, ThemeMode m = ThemeMode::Light,
          ThemeFeatures feat = ThemeFeatures())
        : family(f), mode(m), features(feat) {}

    bool isDark() const { return mode == ThemeMode::Dark; }

    bool operator==(const Theme &other) const {
        return family == other.family && mode == other.mode && features == other.features;
    }
    bool operator!=(const Theme &other) const { return !(*this == other); }
};

// Legacy conversion helpers
inline int themeToLegacyInt(const Theme &t) {
    return static_cast<int>(t.family) * 2 + static_cast<int>(t.mode);
}

inline Theme themeFromLegacyInt(int v) {
    return Theme(static_cast<ThemeColorFamily>(v / 2),
                 static_cast<ThemeMode>(v % 2));
}

#endif // THEMEDEFS_H
