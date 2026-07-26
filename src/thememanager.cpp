#include "thememanager.h"
#include <QApplication>
#include <QPalette>
#include <QColor>
#include <QFont>
#include <QStyle>
#include <QSyntaxHighlighter>
#include <QDebug>

// ── Helper to build a shared suffix (light/dark) ─────────────────────
namespace {

// Shared syntax colours: light mode
ThemeDefinition lightSyntax()
{
    ThemeDefinition d;
    d.synKeyword      = QColor("#1d4ed8");
    d.synString       = QColor("#15803d");
    d.synComment      = QColor("#64748b");
    d.synNumber       = QColor("#9333ea");
    d.synPreprocessor = QColor("#7e22ce");
    d.synTag          = QColor("#2563eb");
    d.synAttribute    = QColor("#a16207");
    d.synCssProperty  = QColor("#0f766e");
    d.synVariable     = QColor("#0369a1");
    d.synFunction     = QColor("#d97706");
    d.synEscape       = QColor("#0e7490");
    return d;
}

// Shared syntax colours: dark mode
ThemeDefinition darkSyntax()
{
    ThemeDefinition d;
    d.synKeyword      = QColor("#93c5fd");
    d.synString       = QColor("#86efac");
    d.synComment      = QColor("#94a3b8");
    d.synNumber       = QColor("#c084fc");
    d.synPreprocessor = QColor("#a855f7");
    d.synTag          = QColor("#60a5fa");
    d.synAttribute    = QColor("#fbbf24");
    d.synCssProperty  = QColor("#2dd4bf");
    d.synVariable     = QColor("#38bdf8");
    d.synFunction     = QColor("#f97316");
    d.synEscape       = QColor("#22d3ee");
    return d;
}

} // anonymous namespace

// ── Build a complete ThemeDefinition from family + mode ──────────────
ThemeDefinition ThemeManager::buildDefinition(ColorFamily family, Mode mode)
{
    ThemeDefinition def;
    const bool dark = (mode == Mode::Dark);

    // Pick the base background colour for the chosen family
    auto bgOf = [](ColorFamily f, bool d) -> QColor {
        switch (f) {
        case ColorFamily::Default: return d ? QColor(45, 45, 48)    : QColor(245, 245, 247);
        case ColorFamily::Blue:    return d ? QColor(25, 35, 50)    : QColor(240, 248, 255);
        case ColorFamily::Green:   return d ? QColor(25, 45, 30)    : QColor(240, 255, 240);
        case ColorFamily::Red:     return d ? QColor(45, 25, 25)    : QColor(255, 245, 245);
        case ColorFamily::Yellow:  return d ? QColor(45, 45, 25)    : QColor(255, 255, 240);
        case ColorFamily::Brown:   return d ? QColor(40, 30, 20)    : QColor(255, 250, 240);
        case ColorFamily::Cyan:    return d ? QColor(25, 45, 45)    : QColor(240, 255, 255);
        case ColorFamily::Violet:  return d ? QColor(35, 25, 50)    : QColor(245, 240, 255);
        }
        return d ? QColor(45, 45, 48) : QColor(245, 245, 247);
    };

    auto accentOf = [](ColorFamily f, bool d) -> QColor {
        switch (f) {
        case ColorFamily::Default: return d ? QColor(100, 150, 255) : QColor(0, 122, 255);
        case ColorFamily::Blue:    return d ? QColor(80, 130, 230)  : QColor(0, 100, 230);
        case ColorFamily::Green:   return d ? QColor(80, 190, 120)  : QColor(0, 160, 80);
        case ColorFamily::Red:     return d ? QColor(220, 80, 80)   : QColor(200, 50, 50);
        case ColorFamily::Yellow:  return d ? QColor(200, 180, 60)  : QColor(180, 160, 0);
        case ColorFamily::Brown:   return d ? QColor(180, 140, 80)  : QColor(160, 120, 60);
        case ColorFamily::Cyan:    return d ? QColor(60, 190, 200)  : QColor(0, 170, 180);
        case ColorFamily::Violet:  return d ? QColor(160, 100, 220) : QColor(140, 60, 200);
        }
        return d ? QColor(100, 150, 255) : QColor(0, 122, 255);
    };

    // ── Core UI colours ───────────────────────────────────────────
    def.bgColor        = bgOf(family, dark);
    def.textColor      = dark ? QColor(220, 220, 220) : QColor(30, 30, 32);
    def.svgColor       = dark ? QColor(180, 180, 190) : QColor(80, 80, 90);
    def.buttonColor    = dark ? QColor(60, 60, 65)    : QColor(240, 240, 242);
    def.highlightColor = accentOf(family, dark);

    // ── Fine-grained UI colours ───────────────────────────────────
    def.baseColor         = dark ? QColor(30, 30, 32)     : QColor(255, 255, 255);
    def.borderColor       = dark ? QColor(60, 60, 65)     : QColor(200, 200, 204);
    def.lightColor        = dark ? QColor(75, 75, 80)     : QColor(230, 230, 233);
    def.midColor          = dark ? QColor(55, 55, 60)     : QColor(195, 195, 199);
    def.darkColor         = dark ? QColor(35, 35, 38)     : QColor(175, 175, 179);
    def.buttonTextColor   = def.textColor;
    def.highlightedText   = dark ? Qt::white : Qt::white;

    // ── Syntax colours ────────────────────────────────────────────
    ThemeDefinition syn = dark ? darkSyntax() : lightSyntax();
    def.synKeyword      = syn.synKeyword;
    def.synString       = syn.synString;
    def.synComment      = syn.synComment;
    def.synNumber       = syn.synNumber;
    def.synPreprocessor = syn.synPreprocessor;
    def.synTag          = syn.synTag;
    def.synAttribute    = syn.synAttribute;
    def.synCssProperty  = syn.synCssProperty;
    def.synVariable     = syn.synVariable;
    def.synFunction     = syn.synFunction;
    def.synEscape       = syn.synEscape;

    return def;
}

// ── Build a QPalette from a ThemeDefinition ──────────────────────────
QPalette ThemeManager::buildPalette(const ThemeDefinition &def)
{
    QPalette p;
    p.setColor(QPalette::Window,           def.bgColor);
    p.setColor(QPalette::WindowText,       def.textColor);
    p.setColor(QPalette::Base,             def.baseColor);
    p.setColor(QPalette::AlternateBase,    def.lightColor);
    p.setColor(QPalette::ToolTipBase,      def.baseColor);
    p.setColor(QPalette::ToolTipText,      def.textColor);
    p.setColor(QPalette::Text,             def.textColor);
    p.setColor(QPalette::Button,           def.buttonColor);
    p.setColor(QPalette::ButtonText,       def.buttonTextColor);
    p.setColor(QPalette::BrightText,       Qt::red);
    p.setColor(QPalette::Link,             def.highlightColor);
    p.setColor(QPalette::Highlight,        def.highlightColor);
    p.setColor(QPalette::HighlightedText,  def.highlightedText);
    p.setColor(QPalette::Light,            def.lightColor);
    p.setColor(QPalette::Midlight,         def.midColor);
    p.setColor(QPalette::Dark,             def.darkColor);
    p.setColor(QPalette::Mid,              def.midColor);
    p.setColor(QPalette::Shadow,           def.darkColor);
    return p;
}

// ── ThemeManager implementation ──────────────────────────────────────

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_currentTheme(ColorFamily::Default, Mode::Light)
{
    buildCaches();
    m_currentDefinition = buildDefinition(m_currentTheme.family, m_currentTheme.mode);
}

ThemeManager::~ThemeManager()
{
}

void ThemeManager::buildCaches()
{
    m_backgroundCache.clear();
    m_accentCache.clear();
    for (int f = 0; f < 8; ++f) {
        ColorFamily family = static_cast<ColorFamily>(f);
        for (int m = 0; m < 2; ++m) {
            Mode mode = static_cast<Mode>(m);
            m_backgroundCache[family][mode] = familyColor(family, mode);
            m_accentCache[family][mode]     = familyColor(family, mode);
        }
    }
}

QColor ThemeManager::familyColor(ColorFamily family, Mode mode, int alpha) const
{
    ThemeDefinition d = buildDefinition(family, mode);
    QColor c = d.bgColor;
    if (alpha != 255) c.setAlpha(alpha);
    return c;
}

void ThemeManager::applyTheme(const Theme &theme)
{
    m_currentTheme = theme;
    m_currentDefinition = buildDefinition(theme.family, theme.mode);

    // Apply high-contrast adjustments if needed
    if (theme.features.testFlag(Feature::HighContrast)) {
        // Boost contrast: make colours more extreme
        bool dark = theme.isDark();
        m_currentDefinition.bgColor   = dark ? QColor(0, 0, 0)     : QColor(255, 255, 255);
        m_currentDefinition.textColor = dark ? QColor(255, 255, 255) : QColor(0, 0, 0);
        m_currentDefinition.svgColor  = m_currentDefinition.textColor;
    }

    QApplication::setStyle("Fusion");
    QPalette palette = buildPalette(m_currentDefinition);
    QApplication::setPalette(palette);

    QApplication *app = qobject_cast<QApplication*>(QApplication::instance());
    if (app) {
        app->setStyleSheet(generateGlobalStylesheet());
    }

    emit themeChanged(theme);
}

void ThemeManager::setCurrentTheme(const Theme &theme)
{
    if (m_currentTheme != theme) {
        applyTheme(theme);
    }
}

QPalette ThemeManager::buildBasePalette(ColorFamily family, Mode mode) const
{
    return buildPalette(buildDefinition(family, mode));
}

QColor ThemeManager::accentColor() const
{
    return m_currentDefinition.highlightColor;
}

QColor ThemeManager::backgroundColor() const
{
    return m_currentDefinition.bgColor;
}

QColor ThemeManager::svgColor() const
{
    return m_currentDefinition.svgColor;
}

QColor ThemeManager::textColor() const
{
    return m_currentDefinition.textColor;
}

QColor ThemeManager::borderColor() const
{
    return m_currentDefinition.borderColor;
}

QStringList ThemeManager::fontStack() const
{
    return QStringList() << "SF Pro Text" << "Inter" << "Segoe UI" << "Noto Sans" << "DejaVu Sans" << "sans-serif";
}

QStringList ThemeManager::monoFontStack() const
{
    return QStringList() << "SF Mono" << "JetBrains Mono" << "Cascadia Code" << "Fira Code" << "DejaVu Sans Mono" << "Consolas" << "monospace";
}

QString ThemeManager::generateDesignTokens() const
{
    QStringList tokens;
    bool dark = m_currentTheme.isDark();
    const ThemeDefinition &d = m_currentDefinition;

    tokens << QString("--accent: %1;").arg(d.highlightColor.name());
    tokens << QString("--background: %1;").arg(d.bgColor.name());
    tokens << QString("--text: %1;").arg(d.textColor.name());
    tokens << QString("--border: %1;").arg(d.borderColor.name());
    tokens << QString("--svg-color: %1;").arg(d.svgColor.name());
    tokens << QString("--radius-sm: 4px;");
    tokens << QString("--radius-md: 6px;");
    tokens << QString("--radius-lg: 8px;");
    tokens << QString("--spacing-xs: 4px;");
    tokens << QString("--spacing-sm: 8px;");
    tokens << QString("--spacing-md: 12px;");
    tokens << QString("--spacing-lg: 16px;");

    return tokens.join("\n  ");
}

QString ThemeManager::generateGlobalStylesheet() const
{
    QStringList tokens = generateDesignTokens().split("\n");
    QStringList lines;
    const ThemeDefinition &d = m_currentDefinition;

    lines << QString("/* Design Tokens */");
    lines << tokens;
    lines << QString("");
    lines << QString("/* Xcode-Inspired Base Styles */");
    lines << QString("QMainWindow, QWidget {");
    lines << QString("    background-color: palette(window);");
    lines << QString("    font-family: %1;").arg(fontStack().join(", "));
    lines << QString("    font-size: 13px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QPlainTextEdit, QTextEdit {");
    lines << QString("    font-family: %1;").arg(monoFontStack().join(", "));
    lines << QString("    font-size: 13px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Group Boxes */");
    lines << QString("QGroupBox {");
    lines << QString("    border: 1px solid palette(mid);");
    lines << QString("    border-radius: 12px;");
    lines << QString("    margin-top: 14px;");
    lines << QString("    padding-top: 14px;");
    lines << QString("    background-color: palette(base);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QGroupBox::title {");
    lines << QString("    subcontrol-origin: margin;");
    lines << QString("    subcontrol-position: top left;");
    lines << QString("    left: 10px;");
    lines << QString("    padding: 0 6px;");
    lines << QString("    color: palette(text);");
    lines << QString("    font-weight: 600;");
    lines << QString("    font-size: 12px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Menu Bar & Toolbar */");
    lines << QString("QMenuBar {");
    lines << QString("    background-color: transparent;");
    lines << QString("    border: none;");
    lines << QString("    padding: 2px 4px;");
    lines << QString("    spacing: 2px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QToolBar {");
    lines << QString("    background-color: transparent;");
    lines << QString("    border: none;");
    lines << QString("    padding: 4px 6px;");
    lines << QString("    spacing: 4px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QToolBar::separator {");
    lines << QString("    background-color: palette(mid);");
    lines << QString("    width: 1px;");
    lines << QString("    margin: 4px 6px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Status Bar */");
    lines << QString("QStatusBar {");
    lines << QString("    background-color: palette(window);");
    lines << QString("    border-top: 1px solid palette(mid);");
    lines << QString("    padding: 2px 10px;");
    lines << QString("    color: palette(text);");
    lines << QString("    font-size: 12px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Menus */");
    lines << QString("QMenu {");
    lines << QString("    background-color: palette(base);");
    lines << QString("    border: 1px solid palette(mid);");
    lines << QString("    border-radius: 10px;");
    lines << QString("    padding: 6px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QMenuBar::item {");
    lines << QString("    background: transparent;");
    lines << QString("    padding: 5px 10px;");
    lines << QString("    border-radius: 6px;");
    lines << QString("    color: palette(text);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QMenuBar::item:selected {");
    lines << QString("    background-color: palette(light);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QMenu::item {");
    lines << QString("    background-color: transparent;");
    lines << QString("    padding: 6px 28px 6px 12px;");
    lines << QString("    border-radius: 6px;");
    lines << QString("    color: palette(text);");
    lines << QString("    margin: 1px 2px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QMenu::item:selected {");
    lines << QString("    background-color: palette(highlight);");
    lines << QString("    color: palette(highlighted-text);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QMenu::separator {");
    lines << QString("    height: 1px;");
    lines << QString("    background-color: palette(mid);");
    lines << QString("    margin: 5px 8px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Tabs — Xcode-style */");
    lines << QString("QTabBar {");
    lines << QString("    background-color: transparent;");
    lines << QString("    border: none;");
    lines << QString("    qproperty-drawBase: false;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QTabBar::tab {");
    lines << QString("    border: none;");
    lines << QString("    border-bottom: 2px solid transparent;");
    lines << QString("    background-color: transparent;");
    lines << QString("    padding: 8px 16px;");
    lines << QString("    margin: 0;");
    lines << QString("    color: palette(mid);");
    lines << QString("    min-width: 70px;");
    lines << QString("    font-size: 12px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QTabBar::tab:selected {");
    lines << QString("    background-color: palette(base);");
    lines << QString("    border-bottom: 2px solid palette(highlight);");
    lines << QString("    color: palette(text);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QTabBar::tab:hover:!selected {");
    lines << QString("    background-color: palette(light);");
    lines << QString("    color: palette(text);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QTabWidget::pane {");
    lines << QString("    border: none;");
    lines << QString("    border-top: 1px solid palette(mid);");
    lines << QString("    background-color: palette(base);");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Buttons — neumorphic */");
    lines << QString("QPushButton,");
    lines << QString("QDialogButtonBox > QPushButton {");
    lines << QString("    border: 1px solid palette(mid);");
    lines << QString("    border-radius: 8px;");
    lines << QString("    padding: 7px 16px;");
    lines << QString("    background-color: palette(button);");
    lines << QString("    color: palette(text);");
    lines << QString("    min-height: 20px;");
    lines << QString("    min-width: 72px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QPushButton:hover,");
    lines << QString("QDialogButtonBox > QPushButton:hover {");
    lines << QString("    background-color: palette(light);");
    lines << QString("    border-color: palette(highlight);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QPushButton:pressed,");
    lines << QString("QDialogButtonBox > QPushButton:pressed {");
    lines << QString("    background-color: palette(mid);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QPushButton:default {");
    lines << QString("    background-color: palette(highlight);");
    lines << QString("    color: palette(highlighted-text);");
    lines << QString("    border-color: palette(highlight);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QPushButton:disabled,");
    lines << QString("QDialogButtonBox > QPushButton:disabled {");
    lines << QString("    background-color: palette(button);");
    lines << QString("    border-color: palette(mid);");
    lines << QString("    color: palette(mid);");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Tool Buttons */");
    lines << QString("QToolButton {");
    lines << QString("    border: none;");
    lines << QString("    border-radius: 6px;");
    lines << QString("    padding: 5px;");
    lines << QString("    background-color: transparent;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QToolButton:hover {");
    lines << QString("    background-color: palette(light);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QToolButton:pressed {");
    lines << QString("    background-color: palette(mid);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QToolButton:checked {");
    lines << QString("    background-color: palette(highlight);");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Inputs */");
    lines << QString("QLineEdit,");
    lines << QString("QComboBox,");
    lines << QString("QSpinBox,");
    lines << QString("QDoubleSpinBox {");
    lines << QString("    border: 1px solid palette(mid);");
    lines << QString("    border-radius: 6px;");
    lines << QString("    padding: 6px 10px;");
    lines << QString("    background-color: palette(base);");
    lines << QString("    color: palette(text);");
    lines << QString("    selection-background-color: palette(highlight);");
    lines << QString("    selection-color: palette(highlighted-text);");
    lines << QString("    min-height: 18px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QLineEdit:focus,");
    lines << QString("QComboBox:focus,");
    lines << QString("QSpinBox:focus,");
    lines << QString("QDoubleSpinBox:focus {");
    lines << QString("    border: 1px solid palette(highlight);");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Tree View */");
    lines << QString("QTreeView,");
    lines << QString("QListView {");
    lines << QString("    border: none;");
    lines << QString("    border-radius: 6px;");
    lines << QString("    background-color: transparent;");
    lines << QString("    outline: none;");
    lines << QString("    padding: 2px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QTreeView::item,");
    lines << QString("QListView::item {");
    lines << QString("    padding: 5px 6px;");
    lines << QString("    border-radius: 6px;");
    lines << QString("    color: palette(text);");
    lines << QString("    border: none;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QTreeView::item:hover,");
    lines << QString("QListView::item:hover {");
    lines << QString("    background-color: palette(light);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QTreeView::item:selected,");
    lines << QString("QListView::item:selected {");
    lines << QString("    background-color: palette(highlight);");
    lines << QString("    color: palette(highlighted-text);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QTreeView::branch {");
    lines << QString("    background: transparent;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QHeaderView::section {");
    lines << QString("    background-color: palette(window);");
    lines << QString("    border: none;");
    lines << QString("    border-bottom: 1px solid palette(mid);");
    lines << QString("    padding: 6px 8px;");
    lines << QString("    color: palette(text);");
    lines << QString("    font-weight: 600;");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Scrollbars */");
    lines << QString("QScrollBar:vertical {");
    lines << QString("    background: transparent;");
    lines << QString("    width: 10px;");
    lines << QString("    margin: 0;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QScrollBar:horizontal {");
    lines << QString("    background: transparent;");
    lines << QString("    height: 10px;");
    lines << QString("    margin: 0;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QScrollBar::handle:vertical {");
    lines << QString("    background-color: palette(mid);");
    lines << QString("    border-radius: 5px;");
    lines << QString("    min-height: 30px;");
    lines << QString("    margin: 2px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QScrollBar::handle:horizontal {");
    lines << QString("    background-color: palette(mid);");
    lines << QString("    border-radius: 5px;");
    lines << QString("    min-width: 30px;");
    lines << QString("    margin: 2px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QScrollBar::handle:hover {");
    lines << QString("    background-color: palette(dark);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QScrollBar::add-line,");
    lines << QString("QScrollBar::sub-line {");
    lines << QString("    height: 0px;");
    lines << QString("    width: 0px;");
    lines << QString("    border: none;");
    lines << QString("    background: transparent;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QScrollBar::add-page,");
    lines << QString("QScrollBar::sub-page {");
    lines << QString("    background: transparent;");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Checkboxes & Radios */");
    lines << QString("QCheckBox,");
    lines << QString("QRadioButton {");
    lines << QString("    spacing: 8px;");
    lines << QString("    color: palette(text);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QCheckBox::indicator,");
    lines << QString("QRadioButton::indicator {");
    lines << QString("    width: 16px;");
    lines << QString("    height: 16px;");
    lines << QString("    border-radius: 4px;");
    lines << QString("    border: 1px solid palette(mid);");
    lines << QString("    background-color: palette(base);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QRadioButton::indicator {");
    lines << QString("    border-radius: 8px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QCheckBox::indicator:hover,");
    lines << QString("QRadioButton::indicator:hover {");
    lines << QString("    border-color: palette(highlight);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QCheckBox::indicator:checked {");
    lines << QString("    background-color: palette(highlight);");
    lines << QString("    border-color: palette(highlight);");
    lines << QString("    image: url(:/icons/check.svg);");
    lines << QString("}");
    lines << QString("");
    lines << QString("QRadioButton::indicator:checked {");
    lines << QString("    background-color: palette(highlight);");
    lines << QString("    border-color: palette(highlight);");
    lines << QString("    background-color: qradialgradient(cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, stop:0.35 palette(highlighted-text), stop:0.4 palette(highlight));");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Tooltips */");
    lines << QString("QToolTip {");
    lines << QString("    background-color: palette(base);");
    lines << QString("    color: palette(text);");
    lines << QString("    border: 1px solid palette(mid);");
    lines << QString("    border-radius: 8px;");
    lines << QString("    padding: 5px 9px;");
    lines << QString("    font-size: 12px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Focus */");
    lines << QString(":focus {");
    lines << QString("    outline: none;");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Primary Button */");
    lines << QString("QPushButton#primaryButton {");
    lines << QString("    background-color: palette(highlight);");
    lines << QString("    color: palette(highlighted-text);");
    lines << QString("    border: none;");
    lines << QString("    border-radius: 10px;");
    lines << QString("    padding: 9px 18px;");
    lines << QString("    font-weight: 600;");
    lines << QString("}");
    lines << QString("");
    lines << QString("QPushButton#primaryButton:hover {");
    lines << QString("    background-color: palette(highlight);");
    lines << QString("    border: none;");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Animations / Transitions */");
    lines << QString("QWidget[animated=\"true\"] {");
    lines << QString("    transition: all 150ms ease-in-out;");
    lines << QString("}");

    return lines.join("\n");
}
