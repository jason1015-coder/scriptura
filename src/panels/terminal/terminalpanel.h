#ifndef SCRIPTURA_TERMINALPANEL_H
#define SCRIPTURA_TERMINALPANEL_H

#include <QColor>
#include <QList>
#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QStackedWidget;
class QTabBar;
class QToolButton;
class TerminalEmulator;
class TerminalSession;
class TerminalView;

/**
 * @brief One terminal: a child process, its screen, and the view that paints it.
 *
 * The three objects are parented to the tab, so closing a tab tears the whole
 * terminal down. The view only borrows the emulator; the tab owns both.
 */
class TerminalTab : public QObject
{
    Q_OBJECT
public:
    TerminalTab(const QString &program, const QStringList &args, const QString &cwd,
                const QProcessEnvironment &env, QWidget *viewParent);
    ~TerminalTab() override;

    TerminalSession *session() const { return m_session; }
    TerminalView *view() const { return m_view; }
    TerminalEmulator *emulator() const { return m_emulator; }

    /// Text for the tab: the program's OSC title, else the shell's name.
    QString displayTitle() const;
    bool isFinished() const { return m_finished; }
    QString statusText() const;

    /// Asks the child to quit and stops the session feeding this tab.
    void shutdown();

    /**
     * Hands the view over to the caller, which then owns it. Closing a tab
     * removes the view from the stack and deletes it right away, so the tab must
     * forget about it before it is destroyed.
     */
    void releaseView();

signals:
    void titleChanged(TerminalTab *tab);
    void finished(TerminalTab *tab, int exitCode);

private slots:
    void onDataReceived(const QByteArray &data);
    void onSessionTitleChanged(const QString &title);
    void onEmulatorTitleChanged(const QString &title);
    void onSessionFinished(int exitCode);
    void onSessionError(const QString &message);

private:
    void writeToSession(const QByteArray &data);

    TerminalSession *m_session = nullptr;
    TerminalEmulator *m_emulator = nullptr;
    TerminalView *m_view = nullptr;
    bool m_finished = false;
    QString m_error;
};

/**
 * @brief The bottom-panel terminal: a tab bar of terminals plus their views.
 *
 * The first terminal is started lazily, the first time the panel is shown, so
 * opening the editor does not spawn a shell that may never be used.
 */
class TerminalPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalPanel(QWidget *parent = nullptr);
    ~TerminalPanel() override;

    /// Directory new terminals start in (the project folder while one is open).
    void setWorkingDirectory(const QString &directory);
    QString workingDirectory() const { return m_workingDirectory; }

    /// Terminal text and background, taken from the editor theme.
    void setTerminalColors(const QColor &foreground, const QColor &background);
    QColor foregroundColor() const { return m_foreground; }
    QColor backgroundColor() const { return m_background; }

    void setFontSize(int pointSize);
    int fontSize() const { return m_fontSize; }

    int sessionCount() const { return m_tabs.size(); }
    TerminalView *currentView() const;
    QString currentTitle() const;

    /// Shell used for terminals created from the panel's picker.
    QString shellProgram() const;
    void setShellProgram(const QString &program);

    /// Starts a terminal. With no program the configured shell is used.
    TerminalTab *createSession(const QString &program = QString(),
                               const QStringList &args = QStringList());
    void closeSession(int index);
    void closeCurrentSession();

    /// Types a command into the active terminal and runs it.
    bool runCommand(const QString &command, bool sendNewline = true);

    /// Types text without running it (used for pasting into a prompt).
    void sendText(const QString &text);

    void focusTerminal();

signals:
    void sessionTitleChanged(const QString &title);
    void sessionCountChanged(int count);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onNewSessionClicked();
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    void onShellChosen(int index);
    void onTerminateClicked();
    void onZoomInClicked();
    void onZoomOutClicked();

private:
    TerminalTab *tabAt(int index) const;
    void populateShellCombo();
    void refreshTab(int index);
    void updateTabTitle(int index);
    void updateActionStates();
    void applyViewSettings(TerminalTab *tab);

    QTabBar *m_tabBar = nullptr;
    QToolButton *m_newButton = nullptr;
    QComboBox *m_shellCombo = nullptr;
    QToolButton *m_terminateButton = nullptr;
    QToolButton *m_zoomInButton = nullptr;
    QToolButton *m_zoomOutButton = nullptr;
    QStackedWidget *m_stack = nullptr;

    QList<TerminalTab *> m_tabs;

    QString m_workingDirectory;
    QString m_shellProgram;
    QColor m_foreground;
    QColor m_background;
    int m_fontSize = 12;
};

#endif // SCRIPTURA_TERMINALPANEL_H