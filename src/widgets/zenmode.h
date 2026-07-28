#ifndef ZENMODE_H
#define ZENMODE_H

#include <QObject>
#include <QWidget>
#include <QTimer>

class QMainWindow;
class QPlainTextEdit;

/**
 * Zen Mode provides a distraction-free, full-screen editing experience.
 * Features:
 * - Full-screen mode with hidden panels
 * - Optional line focus (dimming non-current lines)
 * - Auto-hide cursor when idle
 * - Centered text with optimal line width
 */
class ZenMode : public QObject
{
    Q_OBJECT
public:
    explicit ZenMode(QMainWindow *mainWindow, QPlainTextEdit *editor = nullptr, QObject *parent = nullptr);
    void setEditor(QPlainTextEdit *editor) { m_editor = editor; }

    // Core operations
    void toggle();
    void enter();
    void exit();
    bool isActive() const { return m_active; }
    
    // Configuration
    void setCentered(bool centered);
    bool isCentered() const { return m_centered; }
    
    void setLineFocus(bool enabled);
    bool isLineFocus() const { return m_lineFocus; }
    
    void setDimOpacity(int opacity);  // 0-255
    int dimOpacity() const { return m_dimOpacity; }
    
    void setAutoHideCursor(bool enabled);
    bool isAutoHideCursor() const { return m_autoHideCursor; }
    
    void setMaxLineWidth(int width);  // in characters
    int maxLineWidth() const { return m_maxLineWidth; }

signals:
    void zenModeChanged(bool active);

private slots:
    void onCursorTimer();
    void onCursorMoved();
    
private:
    void saveNormalState();
    void restoreNormalState();
    void applyZenState();
    void updateLineFocus();
    
    QMainWindow *m_mainWindow;
    QPlainTextEdit *m_editor;
    
    bool m_active;
    bool m_centered;
    bool m_lineFocus;
    bool m_autoHideCursor;
    int m_dimOpacity;
    int m_maxLineWidth;
    
    // Saved state before entering zen mode
    bool m_wasMaximized;
    QWidget *m_savedSidebar;
    QWidget *m_savedBottomPanel;
    QWidget *m_savedTitleBar;
    QWidget *m_savedStatusBar;
    
    // Cursor auto-hide
    QTimer *m_cursorTimer;
    bool m_cursorHidden;
};

#endif // ZENMODE_H
