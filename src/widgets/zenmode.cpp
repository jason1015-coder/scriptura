#include "zenmode.h"
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QApplication>
#include <QScreen>
#include <QShortcut>
#include <QKeyEvent>

ZenMode::ZenMode(QMainWindow *mainWindow, QPlainTextEdit *editor, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_editor(editor)
    , m_active(false)
    , m_centered(true)
    , m_lineFocus(true)
    , m_autoHideCursor(true)
    , m_dimOpacity(60)
    , m_maxLineWidth(80)
    , m_wasMaximized(false)
    , m_savedSidebar(nullptr)
    , m_savedBottomPanel(nullptr)
    , m_savedTitleBar(nullptr)
    , m_savedStatusBar(nullptr)
    , m_cursorTimer(new QTimer(this))
    , m_cursorHidden(false)
{
    m_cursorTimer->setSingleShot(true);
    m_cursorTimer->setInterval(3000);  // Hide cursor after 3 seconds
    connect(m_cursorTimer, &QTimer::timeout, this, &ZenMode::onCursorTimer);
}

void ZenMode::toggle()
{
    if (m_active) {
        exit();
    } else {
        enter();
    }
}

void ZenMode::enter()
{
    if (m_active) return;
    
    saveNormalState();
    applyZenState();
    
    m_active = true;
    emit zenModeChanged(true);
}

void ZenMode::exit()
{
    if (!m_active) return;
    
    restoreNormalState();
    
    m_active = false;
    emit zenModeChanged(false);
}

void ZenMode::setCentered(bool centered)
{
    m_centered = centered;
    if (m_active) {
        applyZenState();
    }
}

void ZenMode::setLineFocus(bool enabled)
{
    m_lineFocus = enabled;
    if (m_active) {
        updateLineFocus();
    }
}

void ZenMode::setDimOpacity(int opacity)
{
    m_dimOpacity = qBound(0, opacity, 255);
    if (m_active && m_lineFocus) {
        updateLineFocus();
    }
}

void ZenMode::setAutoHideCursor(bool enabled)
{
    m_autoHideCursor = enabled;
    if (!enabled) {
        m_cursorTimer->stop();
        if (m_cursorHidden && m_mainWindow) {
            m_mainWindow->setCursor(Qt::ArrowCursor);
            m_cursorHidden = false;
        }
    }
}

void ZenMode::setMaxLineWidth(int width)
{
    m_maxLineWidth = width;
    if (m_active) {
        applyZenState();
    }
}

void ZenMode::onCursorTimer()
{
    if (m_autoHideCursor && m_active && m_mainWindow) {
        m_mainWindow->setCursor(Qt::BlankCursor);
        m_cursorHidden = true;
    }
}

void ZenMode::onCursorMoved()
{
    if (m_autoHideCursor && m_active) {
        if (m_cursorHidden && m_mainWindow) {
            m_mainWindow->setCursor(Qt::ArrowCursor);
            m_cursorHidden = false;
        }
        m_cursorTimer->start();
    }
}

void ZenMode::saveNormalState()
{
    if (!m_mainWindow) return;
    
    m_wasMaximized = m_mainWindow->isMaximized();
    
    // Find and save sidebar, bottom panel, title bar, status bar
    // This is simplified - in production you'd need to properly identify these widgets
    QList<QWidget*> children = m_mainWindow->findChildren<QWidget*>();
    for (QWidget *w : children) {
        QString name = w->objectName();
        if (name.contains("sidebar", Qt::CaseInsensitive)) {
            m_savedSidebar = w;
        } else if (name.contains("bottom", Qt::CaseInsensitive)) {
            m_savedBottomPanel = w;
        } else if (name.contains("title", Qt::CaseInsensitive)) {
            m_savedTitleBar = w;
        } else if (name.contains("status", Qt::CaseInsensitive)) {
            m_savedStatusBar = w;
        }
    }
}

void ZenMode::restoreNormalState()
{
    if (!m_mainWindow) return;
    
    // Restore visibility of panels
    if (m_savedSidebar) m_savedSidebar->show();
    if (m_savedBottomPanel) m_savedBottomPanel->show();
    if (m_savedTitleBar) m_savedTitleBar->show();
    if (m_savedStatusBar) m_savedStatusBar->show();
    
    // Restore window state
    if (m_wasMaximized) {
        m_mainWindow->showMaximized();
    } else {
        m_mainWindow->showNormal();
    }
    
    // Restore cursor
    if (m_cursorHidden) {
        m_mainWindow->setCursor(Qt::ArrowCursor);
        m_cursorHidden = false;
    }
}

void ZenMode::applyZenState()
{
    if (!m_mainWindow) return;
    
    // Hide panels for distraction-free editing
    if (m_savedSidebar) m_savedSidebar->hide();
    if (m_savedBottomPanel) m_savedBottomPanel->hide();
    if (m_savedTitleBar) m_savedTitleBar->hide();
    if (m_savedStatusBar) m_savedStatusBar->hide();
    
    // Go full screen
    m_mainWindow->showFullScreen();
    
    // Start cursor auto-hide timer
    if (m_autoHideCursor) {
        m_cursorTimer->start();
    }
    
    // Apply centered text if editor is available
    if (m_editor) {
        Q_UNUSED(m_editor);
        // In a real implementation, you'd set the editor's maximum width
        // and center it within the viewport
    }
    
    // Update line focus
    if (m_lineFocus) {
        updateLineFocus();
    }
}

void ZenMode::updateLineFocus()
{
    if (!m_lineFocus || !m_editor) return;
    
    // This would integrate with the CodeEditor to dim non-current lines
    // Implementation depends on CodeEditor's paint event
    // For now, emit a signal that can be connected to the editor
}
