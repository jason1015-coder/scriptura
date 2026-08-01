#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "codeeditor.h"
#include "customtitlebar.h"
#include "windowanimator.h"
#include "thememanager.h"
// pluginContext removed - plugins managed via RustPluginManagerAdapter
#include "themeicons.h"
#include "rust_adapter.h"

#include <QMouseEvent>
#include <QKeyEvent>
#include <QSettings>
#include <QTimer>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QAbstractAnimation>
#include <QProcess>

#ifdef Q_OS_LINUX
#include <QApplication>
#include <QWidget>
#include <QRegion>

static bool enableLinuxBlur(WId windowId, bool darkMode)
{
    if (QApplication *app = qobject_cast<QApplication*>(QApplication::instance())) {
        app->setProperty("kwin_blur", true);
        app->setProperty("kwin_blur_region", QRegion());
        app->setProperty("gtk_application_prefer_dark_theme", darkMode);
    }
    return true;
}
#endif

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == tabBar && event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::MiddleButton) {
            int idx = tabBar->tabAt(me->pos());
            if (idx >= 0 && idx < openFiles.size())
                on_tabWidget_tabCloseRequested(idx);
        }
        return QMainWindow::eventFilter(watched, event);
    }

    // Handle title bar dragging via CustomTitleBar
    if (watched == m_titleBar && m_titleBar) {
        if (event->type() == QEvent::MouseButtonPress) {
            m_titleBar->handleMousePress(static_cast<QMouseEvent*>(event));
            return true;
        } else if (event->type() == QEvent::MouseMove) {
            m_titleBar->handleMouseMove(static_cast<QMouseEvent*>(event));
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_titleBar->stopDrag();
            return true;
        }
    }

    // ── Window resize edges (frameless window support) ────────────
    const int MARGIN = 6;
    if (event->type() == QEvent::MouseMove && watched == this) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        QPoint pos = me->position().toPoint();
        int w = width(), h = height();

        bool left   = pos.x() < MARGIN;
        bool right  = pos.x() > w - MARGIN;
        bool top    = pos.y() < MARGIN;
        bool bottom = pos.y() > h - MARGIN;

        if (left && top)        setCursor(Qt::SizeFDiagCursor);
        else if (right && bottom) setCursor(Qt::SizeFDiagCursor);
        else if (right && top)  setCursor(Qt::SizeBDiagCursor);
        else if (left && bottom) setCursor(Qt::SizeBDiagCursor);
        else if (left || right) setCursor(Qt::SizeHorCursor);
        else if (top || bottom) setCursor(Qt::SizeVerCursor);
        else                    setCursor(Qt::ArrowCursor);

        m_resizeEdge = 0;
        if (left)   m_resizeEdge |= Qt::LeftEdge;
        if (right)  m_resizeEdge |= Qt::RightEdge;
        if (top)    m_resizeEdge |= Qt::TopEdge;
        if (bottom) m_resizeEdge |= Qt::BottomEdge;
        return false; // let other handlers process too
    }

    if (event->type() == QEvent::MouseButtonPress && watched == this && m_resizeEdge) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            m_resizing = true;
            m_resizeStartPos = me->globalPosition().toPoint();
            m_resizeStartGeometry = geometry();
            me->accept();
            return true;
        }
    }

    if (event->type() == QEvent::MouseMove && watched == this && m_resizing) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        QPoint delta = me->globalPosition().toPoint() - m_resizeStartPos;
        QRect g = m_resizeStartGeometry;

        if (m_resizeEdge & Qt::LeftEdge) {
            g.setLeft(g.left() + delta.x());
        }
        if (m_resizeEdge & Qt::RightEdge) {
            g.setRight(g.right() + delta.x());
        }
        if (m_resizeEdge & Qt::TopEdge) {
            g.setTop(g.top() + delta.y());
        }
        if (m_resizeEdge & Qt::BottomEdge) {
            g.setBottom(g.bottom() + delta.y());
        }

        setGeometry(g);
        me->accept();
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease && watched == this) {
        if (m_resizing) {
            m_resizing = false;
            m_resizeEdge = 0;
            setCursor(Qt::ArrowCursor);
            return true;
        }
    }

    // Forward navigation keys to the completion popup when visible
    if (m_completionPopup && m_completionPopup->isVisible() && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        switch (ke->key()) {
            case Qt::Key_Down:
                m_completionPopup->setCurrentRow(qMin(m_completionPopup->currentRow() + 1, m_completionPopup->count() - 1));
                return true;
            case Qt::Key_Up:
                m_completionPopup->setCurrentRow(qMax(m_completionPopup->currentRow() - 1, 0));
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if (m_completionPopup->currentItem())
                    emit m_completionPopup->itemActivated(m_completionPopup->currentItem());
                return true;
            case Qt::Key_Escape:
                hideCompletion();
                return true;
            default:
                break;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setupPluginApis()
{
    // When the active editor tab changes, notify the plugin editor API
    // so it can re-apply decorations, markers, and annotations.
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        Q_UNUSED(index);
        // Plugin editor API notification removed - managed by Rust backend
    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (checkUnsavedChanges()) {
        autoSaveTimer->stop();
        
        // Save window geometry and state
        QSettings settings;
        settings.setValue("mainWindow/geometry", saveGeometry());
        settings.setValue("mainWindow/state", saveState());
        settings.setValue("mainWindow/bottomPanelVisible", ui->bottomPanelContainer->isVisible());
        settings.setValue("mainWindow/bottomPanelIndex", currentBottomPanelIndex());
        settings.setValue("ui/sidebarCollapsed", ui->sidebarDrawer->isHidden());
        
        event->accept();

        // If the user wiped all settings and chose to restart, launch a fresh
        // instance now that the close has been accepted (avoids spawning a
        // duplicate if the close was cancelled above).
        if (m_restartRequested) {
            QProcess::startDetached(QCoreApplication::applicationFilePath(), QStringList());
            m_restartRequested = false;
        }
    } else {
        // Close rejected (e.g. unsaved changes) — drop any pending restart so a
        // later normal close doesn't unexpectedly spawn a fresh instance.
        m_restartRequested = false;
        event->ignore();
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    
    // Apply platform-specific backdrop effects after window is shown
#ifdef Q_OS_WIN
    if (windowHandle()) {
        HWND hwnd = reinterpret_cast<HWND>(winId());
        enableMicaEffect(hwnd, isDarkModeEnabled());
    }
#endif
#ifdef Q_OS_MACOS
    // macOS vibrancy - requires Objective-C++ for full NSVisualEffectView support
    // Currently falls back to translucent background
    setAttribute(Qt::WA_TranslucentBackground, false);
#endif
#ifdef Q_OS_LINUX
    // Linux blur support for KDE/KWin compositor
    enableLinuxBlur(winId(), isDarkModeEnabled());
#endif
}

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>

#ifndef DWMWA_MICA_EFFECT
#define DWMWA_MICA_EFFECT 1029
#endif

void MainWindow::enableMicaEffect(HWND hwnd, bool darkMode)
{
    // Try Mica first (Windows 11 22H2+)
    BOOL useMica = TRUE;
    HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_MICA_EFFECT, &useMica, sizeof(useMica));
    
    if (FAILED(hr)) {
        // Mica not available - just enable dark mode title bar
        BOOL darkModeEnabled = darkMode ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkModeEnabled, sizeof(darkModeEnabled));
    }
    
    // Enable rounded corners (Windows 11)
    int cornerPreference = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
    
    // Enable dark mode for title bar
    BOOL darkModeEnabled = darkMode ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkModeEnabled, sizeof(darkModeEnabled));
}
#endif

#ifdef Q_OS_WIN

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG *msg = static_cast<MSG*>(message);
        if (msg->message == WM_NCHITTEST) {
            if (m_titleBar && m_titleBar->isVisible()) {
                QPoint globalPos(msg->lParam & 0xFFFF, (msg->lParam >> 16) & 0xFFFF);
                QPoint localPos = m_titleBar->mapFromGlobal(globalPos);
                if (m_titleBar->rect().contains(localPos)) {
                    *result = HTCAPTION;
                    return true;
                }
            }
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif


void MainWindow::showKeyboardShortcuts()
{
    // Keyboard shortcuts are now part of the unified settings page
    on_action_editor_settings_triggered();
}

Theme MainWindow::themeFromLegacyInt(int legacy) const
{
    if (legacy < 2) {
        return Theme(ThemeColorFamily::Default, static_cast<ThemeMode>(legacy));
    }
    if (legacy >= 30) {
        return Theme(ThemeColorFamily::Default, static_cast<ThemeMode>(legacy - 30),
                     ThemeFeatures(ThemeFeature::HighContrast));
    }
    int familyIndex = (legacy - 2) / 4 + 1;
    int remainder = (legacy - 2) % 4;
    ThemeMode mode = static_cast<ThemeMode>(remainder / 2);
    bool hc = (remainder % 2) != 0;
    return Theme(static_cast<ThemeColorFamily>(familyIndex), mode,
                 hc ? ThemeFeatures(ThemeFeature::HighContrast) : ThemeFeatures());
}

int MainWindow::themeToLegacyInt(const Theme &theme) const
{
    int family = static_cast<int>(theme.family);
    int mode = static_cast<int>(theme.mode);
    bool hc = theme.features.testFlag(ThemeFeature::HighContrast);

    if (family == 0) {
        return hc ? 30 + mode : mode;
    }
    return 2 + (family - 1) * 4 + mode * 2 + (hc ? 1 : 0);
}

