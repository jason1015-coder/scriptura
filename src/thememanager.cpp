#include "thememanager.h"
#include <QApplication>
#include <QPalette>
#include <QColor>
#include <QFont>
#include <QStyle>
#include <QSyntaxHighlighter>
#include <QDebug>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_currentTheme(ColorFamily::Default, Mode::Light)
{
    buildCaches();
}

ThemeManager::~ThemeManager()
{
}

void ThemeManager::buildCaches()
{
    // Pre-cache colors for all family/mode combinations
    m_backgroundCache.clear();
    m_accentCache.clear();

    for (int f = 0; f < 8; ++f) {
        ColorFamily family = static_cast<ColorFamily>(f);
        for (int m = 0; m < 2; ++m) {
            Mode mode = static_cast<Mode>(m);
            m_backgroundCache[family][mode] = familyColor(family, mode);
            m_accentCache[family][mode] = familyColor(family, mode);
        }
    }
}

QColor ThemeManager::familyColor(ColorFamily family, Mode mode, int alpha) const
{
    bool dark = (mode == Mode::Dark);
    QColor color;

    switch (family) {
    case ColorFamily::Default:
        color = dark ? QColor(45, 45, 45) : QColor(255, 255, 255);
        break;
    case ColorFamily::Blue:
        color = dark ? QColor(25, 35, 50) : QColor(240, 248, 255);
        break;
    case ColorFamily::Green:
        color = dark ? QColor(25, 45, 30) : QColor(240, 255, 240);
        break;
    case ColorFamily::Red:
        color = dark ? QColor(45, 25, 25) : QColor(255, 245, 245);
        break;
    case ColorFamily::Yellow:
        color = dark ? QColor(45, 45, 25) : QColor(255, 255, 240);
        break;
    case ColorFamily::Brown:
        color = dark ? QColor(40, 30, 20) : QColor(255, 250, 240);
        break;
    case ColorFamily::Cyan:
        color = dark ? QColor(25, 45, 45) : QColor(240, 255, 255);
        break;
    case ColorFamily::Violet:
        color = dark ? QColor(35, 25, 50) : QColor(245, 240, 255);
        break;
    }

    if (alpha != 255)
        color.setAlpha(alpha);

    return color;
}

void ThemeManager::applyTheme(const Theme &theme)
{
    m_currentTheme = theme;
    QApplication::setStyle("Fusion");
    QPalette palette = buildBasePalette(theme.family, theme.mode);
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
    QPalette palette;
    bool dark = (mode == Mode::Dark);

    QColor windowColor = familyColor(family, mode);
    QColor baseColor = dark ? QColor(30, 30, 30) : QColor(255, 255, 255);
    QColor textColor = dark ? QColor(220, 220, 220) : QColor(30, 30, 30);
    QColor highlight = familyColor(family, mode);
    QColor midColor = dark ? QColor(60, 60, 60) : QColor(200, 200, 200);

    palette.setColor(QPalette::Window, windowColor);
    palette.setColor(QPalette::WindowText, textColor);
    palette.setColor(QPalette::Base, baseColor);
    palette.setColor(QPalette::AlternateBase, dark ? QColor(45, 45, 45) : QColor(245, 245, 245));
    palette.setColor(QPalette::ToolTipBase, baseColor);
    palette.setColor(QPalette::ToolTipText, textColor);
    palette.setColor(QPalette::Text, textColor);
    palette.setColor(QPalette::Button, windowColor);
    palette.setColor(QPalette::ButtonText, textColor);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, highlight);
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::HighlightedText, dark ? Qt::white : Qt::black);
    palette.setColor(QPalette::Light, dark ? QColor(70, 70, 70) : QColor(230, 230, 230));
    palette.setColor(QPalette::Midlight, dark ? QColor(60, 60, 60) : QColor(220, 220, 220));
    palette.setColor(QPalette::Dark, dark ? QColor(20, 20, 20) : QColor(180, 180, 180));
    palette.setColor(QPalette::Mid, midColor);
    palette.setColor(QPalette::Shadow, dark ? QColor(0, 0, 0) : QColor(150, 150, 150));

    return palette;
}

QColor ThemeManager::accentColor() const
{
    return familyColor(m_currentTheme.family, m_currentTheme.mode);
}

QColor ThemeManager::backgroundColor() const
{
    return familyColor(m_currentTheme.family, m_currentTheme.mode);
}

QColor ThemeManager::textColor() const
{
    return m_currentTheme.isDark() ? QColor(220, 220, 220) : QColor(30, 30, 30);
}

QColor ThemeManager::borderColor() const
{
    return m_currentTheme.isDark() ? QColor(80, 80, 80) : QColor(200, 200, 200);
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
    QColor accent = accentColor();
    QColor bg = backgroundColor();
    QColor text = textColor();
    QColor border = borderColor();

    tokens << QString("--accent: %1;").arg(accent.name());
    tokens << QString("--background: %1;").arg(bg.name());
    tokens << QString("--text: %1;").arg(text.name());
    tokens << QString("--border: %1;").arg(border.name());
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
    lines << QString("QLineEdit:disabled,");
    lines << QString("QComboBox:disabled,");
    lines << QString("QSpinBox:disabled {");
    lines << QString("    border-color: palette(mid);");
    lines << QString("    color: palette(mid);");
    lines << QString("    background-color: palette(window);");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Text Editors */");
    lines << QString("QPlainTextEdit,");
    lines << QString("QTextEdit {");
    lines << QString("    border: 1px solid palette(mid);");
    lines << QString("    border-radius: 6px;");
    lines << QString("    padding: 6px;");
    lines << QString("    background-color: palette(base);");
    lines << QString("    color: palette(text);");
    lines << QString("    selection-background-color: palette(highlight);");
    lines << QString("    selection-color: palette(highlighted-text);");
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
    lines << QString("/* Scrollbars (thin, overlay-style) */");
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
    lines << QString("/* Tooltips — glassmorphism */");
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
    lines << QString("/* Welcome Title */");
    lines << QString("QLabel#welcomeTitle {");
    lines << QString("    color: palette(text);");
    lines << QString("    letter-spacing: 0.5px;");
    lines << QString("}");
    lines << QString("");
    lines << QString("/* Animations / Transitions */");
    lines << QString("QWidget[animated=\"true\"] {");
    lines << QString("    transition: all 150ms ease-in-out;");
    lines << QString("}");

    return lines.join("\n");
}
