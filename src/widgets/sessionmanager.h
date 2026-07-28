#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>

class QMainWindow;
class QTabWidget;

/**
 * Saves and restores the entire editor session across restarts:
 * - Open files, tab order, cursor positions
 * - Unsaved buffer content (hot exit)
 * - Split view layouts and sizes
 * - Bottom panel visibility and selected tab
 * - Sidebar collapsed/expanded state
 */
class SessionManager : public QObject
{
    Q_OBJECT
public:
    explicit SessionManager(QMainWindow *mainWindow, QTabWidget *tabWidget, QObject *parent = nullptr);

    // Save the current session to settings
    void saveSession();

    // Restore a previously saved session
    bool restoreSession();

    // Hot exit: save unsaved buffers
    void saveUnsavedBuffers();

    // Restore unsaved buffers (hot exit)
    bool restoreUnsavedBuffers();

    // Check if there's a saved session to restore
    bool hasSavedSession() const;

    // Clear the saved session
    void clearSession();

    // Enable/disable auto-save on exit
    void setAutoSaveSession(bool enabled) { m_autoSaveSession = enabled; }
    bool autoSaveSession() const { return m_autoSaveSession; }

    // Enable/disable hot exit (preserve unsaved files)
    void setHotExitEnabled(bool enabled) { m_hotExitEnabled = enabled; }
    bool hotExitEnabled() const { return m_hotExitEnabled; }

signals:
    void sessionRestored();
    void sessionSaved();
    void sessionFileRequested(const QString &filePath, int cursorLine, int cursorColumn, bool activate);
    void hotExitFileRequested(const QString &originalPath, const QString &content, int cursorLine, int cursorColumn);

private:
    struct FileSessionData {
        QString filePath;
        QString content;        // For hot exit of unsaved files
        int cursorLine = 0;
        int cursorColumn = 0;
        bool modified = false;
        bool isUntitled = false;
    };

    QJsonObject serializeEditorState(int tabIndex);
    void restoreEditorState(const QJsonObject &state, int tabIndex);
    QString sessionFilePath() const;
    QString hotExitDir() const;

    QMainWindow *m_mainWindow;
    QTabWidget *m_tabWidget;
    bool m_autoSaveSession = true;
    bool m_hotExitEnabled = true;
};

#endif // SESSIONMANAGER_H
