#include "mainwindow.h"
#include "crashhandler.h"
#include "splashscreen.h"
#include <QFileDialog>
#include <QSettings>
#include "welcomemenuscreen.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QSettings>
#include <QPalette>
#include <QColor>
#include <QTimer>
#include <QFileInfo>

// Theme helper function to get window color based on theme
QColor getThemeWindowColor(ThemeColorFamily family, ThemeMode mode) {
    bool isDark = (mode == ThemeMode::Dark);
    
    if (family == ThemeColorFamily::Default) {
        return isDark ? QColor(53, 53, 53) : QColor(255, 255, 255);
    }
    if (family == ThemeColorFamily::Blue) {
        return isDark ? QColor(25, 35, 50) : QColor(240, 248, 255);
    }
    if (family == ThemeColorFamily::Green) {
        return isDark ? QColor(25, 45, 30) : QColor(240, 255, 240);
    }
    if (family == ThemeColorFamily::Red) {
        return isDark ? QColor(45, 25, 25) : QColor(255, 245, 245);
    }
    if (family == ThemeColorFamily::Yellow) {
        return isDark ? QColor(45, 45, 25) : QColor(255, 255, 240);
    }
    if (family == ThemeColorFamily::Brown) {
        return isDark ? QColor(40, 30, 20) : QColor(255, 250, 240);
    }
    if (family == ThemeColorFamily::Cyan) {
        return isDark ? QColor(25, 45, 45) : QColor(240, 255, 255);
    }
    if (family == ThemeColorFamily::Violet) {
        return isDark ? QColor(35, 25, 50) : QColor(245, 240, 255);
    }
    return isDark ? QColor(53, 53, 53) : QColor(255, 255, 255);
}

// Theme helper function to extract family from legacy theme int
ThemeColorFamily getThemeFamily(int legacy) {
    if (legacy < 2) {
        return ThemeColorFamily::Default;
    }
    if (legacy >= 30) {
        return ThemeColorFamily::Default;
    }
    int familyIndex = (legacy - 2) / 4 + 1;
    return static_cast<ThemeColorFamily>(familyIndex);
}

// Theme helper function to extract mode from legacy theme int
ThemeMode getThemeMode(int legacy) {
    if (legacy < 2) {
        return static_cast<ThemeMode>(legacy);
    }
    if (legacy >= 30) {
        return static_cast<ThemeMode>(legacy - 30);
    }
    int remainder = (legacy - 2) % 4;
    return static_cast<ThemeMode>(remainder / 2);
}

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
// Link with dwmapi.lib
#pragma comment(lib, "dwmapi.lib")

// DWM attributes (may not be in all SDK versions)
#ifndef DWMWA_MICA_EFFECT
#define DWMWA_MICA_EFFECT 1029
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_COLOR_COLORS
#define DWMWA_COLOR_COLORS 1028
#endif

struct DWM_COLOR_PARAMS {
    DWORD dwFlags;
    COLORREF clrCaption;
    COLORREF clrText;
    COLORREF clrHyperlink;
    COLORREF clrCaptionText;
    COLORREF clrCaptionButtonText;
    COLORREF clrCaptionButtonHoverText;
    COLORREF clrCaptionButtonPressedText;
};
#endif // Q_OS_WIN

int main(int argc, char *argv[])
{
    CrashHandler::install();

    QApplication::setOrganizationName("Scriptura");
    QApplication::setApplicationName("Scriptura");
    QApplication::setStyle("Fusion");

    QApplication a(argc, argv);

    // Parse command-line arguments:
    //   scriptura [--project <dir>] [file1 file2 ...]
    // A bare directory argument is treated as the project to open.
    QString initialProject;
    QStringList initialFiles;
    const QStringList cliArgs = a.arguments();
    for (int i = 1; i < cliArgs.size(); ++i) {
        const QString &arg = cliArgs.at(i);
        if (arg == "--project" && i + 1 < cliArgs.size()) {
            initialProject = cliArgs.at(++i);
        } else {
            QFileInfo fi(arg);
            if (fi.isDir()) {
                if (initialProject.isEmpty())
                    initialProject = arg;
            } else {
                initialFiles.append(arg);
            }
        }
    }

    a.setWindowIcon(QIcon(":/icon.png"));

    a.setStyleSheet(R"(
/* ============================================================
   Scriptura — Xcode-Inspired Design System
   Glassmorphism panels, neumorphic controls, unified toolbar.
   ============================================================ */

/* ---- Base ---- */
QMainWindow, QWidget {
    background-color: palette(window);
    font-family: "Inter", "SF Pro Text", "Segoe UI", "Noto Sans", "DejaVu Sans", sans-serif;
    font-size: 13px;
}

/* Monospace surfaces get a code font */
QPlainTextEdit, QTextEdit {
    font-family: "SF Mono", "JetBrains Mono", "Cascadia Code", "Fira Code", "DejaVu Sans Mono", "Consolas", monospace;
    font-size: 13px;
}

/* ---- Group boxes — neumorphic raised ---- */
QGroupBox {
    border: 1px solid palette(mid);
    border-top-color: palette(light);
    border-left-color: palette(light);
    border-radius: 12px;
    margin-top: 14px;
    padding-top: 14px;
    background-color: palette(base);
}

QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 10px;
    padding: 0 6px;
    color: palette(text);
    font-weight: 600;
    font-size: 12px;
}

/* ---- Content surfaces ---- */
QTreeView,
QListView,
QTableView,
QAbstractItemView {
    border: none;
    border-radius: 8px;
    background-color: transparent;
    outline: none;
    padding: 2px;
}

/* ---- Menu bar ---- */
QMenuBar {
    background-color: transparent;
    border: none;
    padding: 2px 4px;
    spacing: 2px;
}

QToolBar {
    background-color: transparent;
    border: none;
    padding: 4px 6px;
    spacing: 4px;
}

QToolBar::separator {
    background-color: palette(mid);
    width: 1px;
    margin: 4px 6px;
}

QStatusBar {
    background-color: palette(window);
    border-top: 1px solid palette(mid);
    padding: 2px 10px;
    color: palette(text);
    font-size: 12px;
}

QStatusBar::item {
    border: none;
    padding: 0 6px;
}

/* ---- Menus — neumorphic raised ---- */
QMenu {
    background-color: palette(base);
    border: 1px solid palette(mid);
    border-top-color: palette(light);
    border-left-color: palette(light);
    border-radius: 12px;
    padding: 6px;
}

QMenuBar::item {
    background: transparent;
    padding: 5px 10px;
    border-radius: 8px;
    color: palette(text);
}

QMenuBar::item:selected {
    background-color: palette(light);
}

QMenuBar::item:pressed {
    background-color: palette(highlight);
    color: palette(highlighted-text);
}

QMenu::item {
    background-color: transparent;
    padding: 6px 28px 6px 12px;
    border-radius: 8px;
    color: palette(text);
    margin: 1px 2px;
}

QMenu::item:selected {
    background-color: palette(highlight);
    color: palette(highlighted-text);
}

QMenu::item:disabled {
    color: palette(mid);
}

QMenu::separator {
    height: 1px;
    background-color: palette(mid);
    margin: 5px 8px;
}

QMenu::icon {
    padding-left: 6px;
}

/* ---- Tabs — Xcode-style, active merges with editor ---- */
QTabBar {
    background-color: transparent;
    border: none;
    qproperty-drawBase: false;
}

QTabBar::tab {
    border: none;
    border-bottom: 2px solid transparent;
    background-color: transparent;
    padding: 8px 16px;
    margin: 0;
    color: palette(mid);
    min-width: 70px;
    font-size: 12px;
    border-top-left-radius: 10px;
    border-top-right-radius: 10px;
}

QTabBar::tab:selected {
    background-color: palette(base);
    border-bottom: 2px solid palette(highlight);
    color: palette(text);
}

QTabBar::tab:hover:!selected {
    background-color: palette(light);
    color: palette(text);
}

QTabBar::tab:disabled {
    color: palette(mid);
}

QTabWidget::pane {
    border: none;
    border-top: 1px solid palette(mid);
    background-color: palette(base);
}

/* ---- Buttons — neumorphic raised ---- */
QPushButton,
QDialogButtonBox > QPushButton {
    border: 1px solid palette(mid);
    border-top-color: palette(light);
    border-left-color: palette(light);
    border-radius: 10px;
    padding: 7px 16px;
    background-color: palette(button);
    color: palette(text);
    min-height: 20px;
    min-width: 72px;
}

QPushButton:hover,
QDialogButtonBox > QPushButton:hover {
    background-color: palette(light);
    border-top-color: palette(highlight);
    border-left-color: palette(highlight);
}

QPushButton:pressed,
QDialogButtonBox > QPushButton:pressed {
    background-color: palette(mid);
    border-top-color: palette(mid);
    border-left-color: palette(mid);
    border-bottom-color: palette(light);
    border-right-color: palette(light);
}

QPushButton:default {
    background: palette(highlight);
    color: palette(highlighted-text);
    border: 1px solid palette(highlight);
}

QPushButton:disabled,
QDialogButtonBox > QPushButton:disabled {
    background-color: palette(button);
    border-color: palette(mid);
    color: palette(mid);
}

/* Accent primary button — neumorphic raised */
QPushButton#primaryButton {
    color: palette(highlighted-text);
    border: 1px solid palette(highlight);
    border-radius: 12px;
    padding: 9px 18px;
    font-weight: 600;
    background: palette(highlight);
}

QPushButton#primaryButton:hover {
    background: palette(highlight);
    border: 1px solid palette(highlight);
}

/* ---- Tool buttons — neumorphic raised ---- */
QToolButton {
    border: none;
    border-radius: 8px;
    padding: 5px;
    background-color: transparent;
    color: palette(text);
}

QToolButton:hover {
    background-color: palette(light);
}

QToolButton:pressed {
    background-color: palette(mid);
}

QToolButton:checked {
    background: palette(highlight);
    border: 1px solid palette(highlight);
    color: palette(highlighted-text);
    border-radius: 8px;
}

QToolButton:disabled {
    color: palette(mid);
}

/* ---- Inputs — neumorphic inset ---- */
QLineEdit,
QComboBox,
QSpinBox,
QDoubleSpinBox {
    border: 1px solid palette(mid);
    border-top-color: palette(dark);
    border-left-color: palette(dark);
    border-bottom-color: palette(light);
    border-right-color: palette(light);
    border-radius: 8px;
    padding: 6px 10px;
    background-color: palette(base);
    color: palette(text);
    selection-background-color: palette(highlight);
    selection-color: palette(highlighted-text);
    min-height: 18px;
}

QLineEdit:focus,
QComboBox:focus,
QSpinBox:focus,
QDoubleSpinBox:focus {
    border: 1px solid palette(highlight);
}

QLineEdit:disabled,
QComboBox:disabled,
QSpinBox:disabled {
    border-color: palette(mid);
    color: palette(mid);
    background-color: palette(window);
}

QComboBox::drop-down {
    border: none;
    width: 20px;
}

QComboBox QAbstractItemView {
    border: 1px solid palette(mid);
    border-radius: 8px;
    background-color: palette(base);
    selection-background-color: palette(highlight);
    selection-color: palette(highlighted-text);
    padding: 4px;
}

/* ---- Text editors ---- */
QPlainTextEdit,
QTextEdit {
    border: 1px solid palette(mid);
    border-radius: 8px;
    padding: 6px;
    background-color: palette(base);
    color: palette(text);
    selection-background-color: palette(highlight);
    selection-color: palette(highlighted-text);
}

QPlainTextEdit:focus,
QTextEdit:focus {
    border: 1px solid palette(highlight);
}

/* ---- Tree / list items ---- */
QTreeView::item,
QListView::item {
    padding: 5px 6px;
    border-radius: 8px;
    color: palette(text);
    border: none;
}

QTreeView::item:hover,
QListView::item:hover {
    background-color: palette(light);
}

QTreeView::item:selected,
QListView::item:selected {
    background-color: palette(highlight);
    color: palette(highlighted-text);
}

QTreeView::item:selected:!active,
QListView::item:selected:!active {
    background-color: palette(mid);
    color: palette(text);
}

QTreeView::branch {
    background: transparent;
}

QHeaderView::section {
    background-color: palette(window);
    border: none;
    border-bottom: 1px solid palette(mid);
    padding: 6px 8px;
    color: palette(text);
    font-weight: 600;
}

/* ---- Scrollbars (thin, overlay-style) ---- */
QScrollBar:vertical {
    background: transparent;
    width: 12px;
    margin: 0;
}

QScrollBar:horizontal {
    background: transparent;
    height: 12px;
    margin: 0;
}

QScrollBar::handle:vertical {
    background-color: palette(mid);
    border-radius: 6px;
    min-height: 30px;
    margin: 2px;
    border: 1px solid palette(mid);
    border-top-color: palette(light);
    border-left-color: palette(light);
}

QScrollBar::handle:horizontal {
    background-color: palette(mid);
    border-radius: 6px;
    min-width: 30px;
    margin: 2px;
    border: 1px solid palette(mid);
    border-top-color: palette(light);
    border-left-color: palette(light);
}

QScrollBar::handle:hover {
    background-color: palette(dark);
}

QScrollBar::add-line,
QScrollBar::sub-line {
    height: 0px;
    width: 0px;
    border: none;
    background: transparent;
}

QScrollBar::add-page,
QScrollBar::sub-page {
    background: transparent;
}

/* ---- Checkboxes & radios ---- */
QCheckBox,
QRadioButton {
    spacing: 8px;
    color: palette(text);
}

QCheckBox::indicator,
QRadioButton::indicator {
    width: 16px;
    height: 16px;
    border-radius: 6px;
    border: 1px solid palette(mid);
    border-top-color: palette(dark);
    border-left-color: palette(dark);
    border-bottom-color: palette(light);
    border-right-color: palette(light);
    background-color: palette(base);
}

QRadioButton::indicator {
    border-radius: 8px;
}

QCheckBox::indicator:hover,
QRadioButton::indicator:hover {
    border-top-color: palette(highlight);
    border-left-color: palette(highlight);
    border-bottom-color: palette(highlight);
    border-right-color: palette(highlight);
}

QCheckBox::indicator:checked {
    background: palette(highlight);
    border: 1px solid palette(highlight);
    image: url(:/icons/check.svg);
}

QRadioButton::indicator:checked {
    background: palette(highlight);
    border: 1px solid palette(highlight);
}

QCheckBox:disabled,
QRadioButton:disabled {
    color: palette(mid);
}

QCheckBox::indicator:disabled,
QRadioButton::indicator:disabled {
    border-color: palette(mid);
    background-color: palette(window);
}

/* ---- Tooltips ---- */
QToolTip {
    background-color: palette(base);
    color: palette(text);
    border: 1px solid palette(mid);
    border-radius: 10px;
    padding: 5px 9px;
    font-size: 12px;
}

/* ---- Spin box buttons ---- */
QSpinBox::up-button,
QSpinBox::down-button,
QDoubleSpinBox::up-button,
QDoubleSpinBox::down-button {
    border: none;
    background-color: transparent;
    width: 16px;
}

QSpinBox::up-button:hover,
QSpinBox::down-button:hover,
QDoubleSpinBox::up-button:hover,
QDoubleSpinBox::down-button:hover {
    background-color: palette(light);
}

/* ---- Progress bars ---- */
QProgressBar {
    border: none;
    border-radius: 6px;
    background-color: palette(mid);
    height: 6px;
    text-align: center;
}

QProgressBar::chunk {
    background-color: palette(highlight);
    border-radius: 6px;
}

/* ---- Splitter handles ---- */
QSplitter::handle {
    background-color: palette(mid);
}

QSplitter::handle:horizontal {
    width: 1px;
}

QSplitter::handle:vertical {
    height: 1px;
}

QSplitter::handle:hover {
    background-color: palette(highlight);
}

/* ---- Dialogs ---- */
QDialog {
    background-color: palette(window);
}

QDialogButtonBox {
    padding: 6px;
}

QDialogButtonBox > QPushButton {
    min-width: 84px;
}

/* ---- Focus ---- */
:focus {
    outline: none;
}

/* ---- Labels ---- */
)");

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "scriptura_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    // Load theme before showing splash screen
    QSettings settings;
    int legacyTheme = settings.value("theme/selected", 0).toInt();
    ThemeColorFamily family = getThemeFamily(legacyTheme);
    ThemeMode mode = getThemeMode(legacyTheme);
    QColor themeWindowColor = getThemeWindowColor(family, mode);

    // Determine if a CLI project was provided — if so, skip the menu screen entirely
    bool hasCliProject = !initialProject.isEmpty();

    if (hasCliProject) {
        // Short splash, then go straight to main window
        SplashScreen *splash = new SplashScreen;
        splash->setThemeBackground(themeWindowColor);
        splash->showWithDelay(1500);

        QTimer::singleShot(1500, [splash, initialProject, initialFiles]() {
            MainWindow *mainWindow = new MainWindow(initialProject, initialFiles);
            mainWindow->show();

            // Now it is safe to close the splash
            splash->close();
            splash->deleteLater();

#ifdef Q_OS_WIN
            HWND hwnd = reinterpret_cast<HWND>(mainWindow->winId());
            mainWindow->enableMicaEffect(hwnd, mainWindow->isDarkModeEnabled());
#endif
        });
    } else {
        // Show splash briefly, then present the WelcomeMenuScreen
        SplashScreen *splash = new SplashScreen;
        splash->setThemeBackground(themeWindowColor);
        splash->showWithDelay(1000);

        QTimer::singleShot(1000, [splash, themeWindowColor]() {
            qDebug() << "[DIAG] splash timer fired; showing WelcomeMenuScreen...";

            splash->close();
            splash->deleteLater();

            WelcomeMenuScreen *welcome = new WelcomeMenuScreen();
            welcome->setThemeBackground(themeWindowColor);

            // Center the welcome screen on the primary screen
            QScreen *screen = QApplication::primaryScreen();
            if (screen) {
                QRect available = screen->availableGeometry();
                int x = (available.width() - welcome->width()) / 2;
                int y = (available.height() - welcome->height()) / 2;
                welcome->move(x, y);
            }

            // When user requests to open a project, launch MainWindow
            QObject::connect(welcome, &WelcomeMenuScreen::openProjectRequested, welcome, [welcome]() {
                QString dirName = QFileDialog::getExistingDirectory(
                    nullptr, WelcomeMenuScreen::tr("Open Project"), QString(),
                    QFileDialog::DontUseNativeDialog);
                if (dirName.isEmpty())
                    return;
                // Save recent project
                QSettings settings;
                QStringList recent = settings.value("recentProjects").toStringList();
                if (!recent.contains(dirName)) {
                    recent.prepend(dirName);
                    while (recent.size() > 10)
                        recent.removeLast();
                    settings.setValue("recentProjects", recent);
                }
                welcome->close();
                welcome->deleteLater();
                MainWindow *mainWindow = new MainWindow(dirName);
                mainWindow->show();
                QScreen *screen = QApplication::primaryScreen();
                if (screen) {
                    QRect available = screen->availableGeometry();
                    mainWindow->resize(qMin(available.width() * 7 / 10, 900),
                                       qMin(available.height() * 7 / 10, 800));
                    int x = (available.width() - mainWindow->width()) / 2;
                    int y = (available.height() - mainWindow->height()) / 2;
                    mainWindow->move(x, y);
                }
#ifdef Q_OS_WIN
                HWND hwnd = reinterpret_cast<HWND>(mainWindow->winId());
                mainWindow->enableMicaEffect(hwnd, mainWindow->isDarkModeEnabled());
#endif
            });

            // When user selects a recent project, launch MainWindow directly
            QObject::connect(welcome, &WelcomeMenuScreen::recentProjectSelected, welcome, [welcome](const QString &path) {
                welcome->close();
                welcome->deleteLater();
                MainWindow *mainWindow = new MainWindow(path);
                mainWindow->show();
                QScreen *screen = QApplication::primaryScreen();
                if (screen) {
                    QRect available = screen->availableGeometry();
                    mainWindow->resize(qMin(available.width() * 7 / 10, 900),
                                       qMin(available.height() * 7 / 10, 800));
                    int x = (available.width() - mainWindow->width()) / 2;
                    int y = (available.height() - mainWindow->height()) / 2;
                    mainWindow->move(x, y);
                }
#ifdef Q_OS_WIN
                HWND hwnd = reinterpret_cast<HWND>(mainWindow->winId());
                mainWindow->enableMicaEffect(hwnd, mainWindow->isDarkModeEnabled());
#endif
            });

            // No explicit quit handler needed — QApplication quits automatically
            // when the last visible window is closed (default WA_QuitOnClose behavior).
            // If the user closes the WelcomeMenuScreen without selecting a project,
            // it's the only window, so the app exits. If a MainWindow was created,
            // the WelcomeMenuScreen closes but the MainWindow keeps the app alive.

            welcome->show();
        });
    }

    return a.exec();
}
