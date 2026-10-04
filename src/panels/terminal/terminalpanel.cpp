#include "terminal/terminalpanel.h"

#include "terminal/terminalemulator.h"
#include "terminal/terminalsession.h"
#include "terminal/terminalwidget.h"

#include "internals/settings_store.h"
#include "themeicons.h"

#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QString shellDisplayName(const QString &program)
{
    return QFileInfo(program).completeBaseName();
}

/// Arguments a shell needs to behave like an interactive login terminal.
QStringList shellArguments(const QString &program)
{
    if (program.endsWith(QLatin1String("pwsh.exe"), Qt::CaseInsensitive)
        || program.endsWith(QLatin1String("powershell.exe"), Qt::CaseInsensitive))
        return {QStringLiteral("-NoLogo")};
    return {};
}

} // namespace

// ── TerminalTab ────────────────────────────────────────────────────────────

TerminalTab::TerminalTab(const QString &program, const QStringList &args, const QString &cwd,
                         const QProcessEnvironment &env, QWidget *viewParent)
    : QObject(nullptr)
{
    m_session = new TerminalSession(this);
    m_emulator = new TerminalEmulator(this);
    m_view = new TerminalView(viewParent);

    m_view->setEmulator(m_emulator);

    connect(m_session, &TerminalSession::dataReceived, this, &TerminalTab::onDataReceived);
    connect(m_session, &TerminalSession::titleChanged, this, &TerminalTab::onSessionTitleChanged);
    connect(m_session, &TerminalSession::finished, this, &TerminalTab::onSessionFinished);
    connect(m_session, &TerminalSession::errorOccurred, this, &TerminalTab::onSessionError);
    connect(m_emulator, &TerminalEmulator::titleChanged, this, &TerminalTab::onEmulatorTitleChanged);
    connect(m_emulator, &TerminalEmulator::requestWrite, this, &TerminalTab::writeToSession);
    connect(m_view, &TerminalView::dataToSend, this, &TerminalTab::writeToSession);
    connect(m_view, &TerminalView::requestResize, this,
            [this](int columns, int rows) {
                if (m_emulator->rows() != rows || m_emulator->columns() != columns)
                    m_emulator->resize(rows, columns);
                m_session->resize(columns, rows);
            });

    m_session->setTitle(program);

    QString error;
    if (!m_session->start(program, args, cwd, env, &error)) {
        m_finished = true;
        m_error = error;
        // The failure belongs on screen, not in a log the user never sees.
        m_emulator->feed(QByteArray("\x1b[31m") + error.toUtf8()
                         + QByteArray("\x1b[0m\r\n"));
    }
}

TerminalTab::~TerminalTab()
{
    // The child must go before the view: the view is torn down first only if it
    // is deleted first, so the process is stopped while both are still alive.
    shutdown();
    if (m_view) {
        // QStackedWidget::addWidget() reparented the view to the panel; detach it
        // first or the widget would be destroyed twice.
        m_view->setParent(nullptr);
        delete m_view;
        m_view = nullptr;
    }
    m_emulator = nullptr;
    m_session = nullptr;
}

void TerminalTab::shutdown()
{
    if (!m_session || !m_session->isRunning())
        return;
    disconnect(m_session, nullptr, this, nullptr);
    m_session->terminate();
}

void TerminalTab::releaseView()
{
    m_view = nullptr;
}

void TerminalTab::writeToSession(const QByteArray &data)
{
    if (m_session)
        m_session->write(data);
}

QString TerminalTab::displayTitle() const
{
    if (!m_error.isEmpty())
        return tr("Failed");
    const QString title = m_session ? m_session->title() : QString();
    return title.isEmpty() ? tr("Terminal") : title;
}

QString TerminalTab::statusText() const
{
    if (!m_error.isEmpty())
        return m_error;
    if (!m_finished)
        return tr("Running");
    return tr("Exited (%1)").arg(m_session ? m_session->exitCode() : -1);
}

void TerminalTab::onDataReceived(const QByteArray &data)
{
    m_emulator->feed(data);
}

void TerminalTab::onSessionTitleChanged(const QString &title)
{
    Q_UNUSED(title);
    emit titleChanged(this);
}

void TerminalTab::onEmulatorTitleChanged(const QString &title)
{
    // OSC 2 titles the window, OSC 0 sets both: prefer the more specific one.
    m_session->setTitle(title);
    emit titleChanged(this);
}

void TerminalTab::onSessionFinished(int exitCode)
{
    if (m_finished)
        return;
    m_finished = true;
    m_emulator->feed(QByteArray("\r\n\x1b[2m[Process exited with code ")
                     + QByteArray::number(exitCode) + QByteArray("]\x1b[0m\r\n"));
    emit finished(this, exitCode);
}

void TerminalTab::onSessionError(const QString &message)
{
    if (m_error.isEmpty())
        m_error = message;
    emit titleChanged(this);
}

// ── TerminalPanel ──────────────────────────────────────────────────────────

TerminalPanel::TerminalPanel(QWidget *parent)
    : QWidget(parent)
    , m_workingDirectory(QDir::homePath())
    , m_foreground(0xD8, 0xD8, 0xD8)
    , m_background(0x1E, 0x1E, 0x1E)
{
    m_shellProgram = SettingsStore::instance()
                         .value(QStringLiteral("terminal/shell"), TerminalSession::defaultShell())
                         .toString();
    if (m_shellProgram.isEmpty())
        m_shellProgram = TerminalSession::defaultShell();
    m_fontSize = SettingsStore::instance()
                     .value(QStringLiteral("terminal/fontSize"), 12)
                     .toInt();

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Toolbar: one tab per terminal, then the shell picker ──────────
    QWidget *toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("terminalToolbar"));
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(4, 4, 4, 2);
    toolbarLayout->setSpacing(4);

    m_tabBar = new QTabBar(toolbar);
    m_tabBar->setObjectName(QStringLiteral("terminalTabBar"));
    m_tabBar->setDrawBase(false);
    m_tabBar->setDocumentMode(true);
    m_tabBar->setElideMode(Qt::ElideRight);
    m_tabBar->setExpanding(false);
    m_tabBar->setTabsClosable(true);
    m_tabBar->setUsesScrollButtons(true);
    m_tabBar->setFixedHeight(26);
    toolbarLayout->addWidget(m_tabBar, 1);

    m_newButton = new QToolButton(toolbar);
    m_newButton->setObjectName(QStringLiteral("terminalNewButton"));
    m_newButton->setText(QStringLiteral("+"));
    m_newButton->setToolTip(tr("New Terminal"));
    m_newButton->setCursor(Qt::PointingHandCursor);
    m_newButton->setFixedSize(24, 22);
    toolbarLayout->addWidget(m_newButton);

    m_shellCombo = new QComboBox(toolbar);
    m_shellCombo->setObjectName(QStringLiteral("terminalShellCombo"));
    m_shellCombo->setToolTip(tr("Shell for new terminals"));
    m_shellCombo->setFixedHeight(22);
    m_shellCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_shellCombo->setMinimumWidth(120);
    toolbarLayout->addWidget(m_shellCombo);

    m_terminateButton = new QToolButton(toolbar);
    m_terminateButton->setObjectName(QStringLiteral("terminalTerminateButton"));
    m_terminateButton->setToolTip(tr("Terminate Process"));
    m_terminateButton->setCursor(Qt::PointingHandCursor);
    m_terminateButton->setFixedSize(24, 22);
    ThemeIcons::instance()->setIcon(m_terminateButton, QStringLiteral(":/icons/close.svg"),
                                    ThemeIcons::Role::Normal, 14);
    toolbarLayout->addWidget(m_terminateButton);

    m_zoomOutButton = new QToolButton(toolbar);
    m_zoomOutButton->setObjectName(QStringLiteral("terminalZoomOutButton"));
    m_zoomOutButton->setText(QStringLiteral("-"));
    m_zoomOutButton->setToolTip(tr("Decrease Font Size"));
    m_zoomOutButton->setCursor(Qt::PointingHandCursor);
    m_zoomOutButton->setFixedSize(24, 22);
    toolbarLayout->addWidget(m_zoomOutButton);

    m_zoomInButton = new QToolButton(toolbar);
    m_zoomInButton->setObjectName(QStringLiteral("terminalZoomInButton"));
    m_zoomInButton->setText(QStringLiteral("+"));
    m_zoomInButton->setToolTip(tr("Increase Font Size"));
    m_zoomInButton->setCursor(Qt::PointingHandCursor);
    m_zoomInButton->setFixedSize(24, 22);
    toolbarLayout->addWidget(m_zoomInButton);

    layout->addWidget(toolbar);

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("terminalStack"));
    layout->addWidget(m_stack, 1);

    connect(m_newButton, &QToolButton::clicked, this, &TerminalPanel::onNewSessionClicked);
    connect(m_tabBar, &QTabBar::currentChanged, this, &TerminalPanel::onTabChanged);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &TerminalPanel::onTabCloseRequested);
    connect(m_shellCombo, &QComboBox::activated, this, &TerminalPanel::onShellChosen);
    connect(m_terminateButton, &QToolButton::clicked, this, &TerminalPanel::onTerminateClicked);
    connect(m_zoomInButton, &QToolButton::clicked, this, &TerminalPanel::onZoomInClicked);
    connect(m_zoomOutButton, &QToolButton::clicked, this, &TerminalPanel::onZoomOutClicked);

    populateShellCombo();
}

void TerminalPanel::populateShellCombo()
{
    QStringList shells = TerminalSession::candidateShells();
    if (!shells.contains(m_shellProgram) && QFileInfo::exists(m_shellProgram))
        shells.prepend(m_shellProgram);

    const QSignalBlocker blocker(m_shellCombo);
    m_shellCombo->clear();
    int selected = -1;
    for (const QString &shell : shells) {
        m_shellCombo->addItem(shellDisplayName(shell), shell);
        if (shell == m_shellProgram)
            selected = m_shellCombo->count() - 1;
    }
    if (selected >= 0)
        m_shellCombo->setCurrentIndex(selected);
    m_shellCombo->setToolTip(tr("Shell for new terminals: %1").arg(m_shellProgram));
}

TerminalPanel::~TerminalPanel()
{
    // The tabs own the shell processes; leaving them running would strand them
    // with no terminal attached once the window is gone.
    qDeleteAll(m_tabs);
    m_tabs.clear();
}

void TerminalPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // Starting a shell before the panel is ever shown would leave an unused
    // process behind for the whole session.
    if (m_tabs.isEmpty())
        createSession();
    else if (TerminalView *view = currentView())
        view->setFocus();
}

void TerminalPanel::setWorkingDirectory(const QString &directory)
{
    if (directory.isEmpty() || directory == m_workingDirectory)
        return;
    if (!QFileInfo(directory).isDir())
        return;
    m_workingDirectory = QDir(directory).absolutePath();
}

void TerminalPanel::setTerminalColors(const QColor &foreground, const QColor &background)
{
    if (foreground.isValid())
        m_foreground = foreground;
    if (background.isValid())
        m_background = background;

    for (TerminalTab *tab : m_tabs) {
        if (TerminalEmulator *emulator = tab->emulator()) {
            emulator->setDefaultForeground(m_foreground);
            emulator->setDefaultBackground(m_background);
            if (TerminalView *view = tab->view())
                view->applyStyle();
        }
    }
}

void TerminalPanel::setFontSize(int pointSize)
{
    const int clamped = qBound(6, pointSize, 32);
    if (clamped == m_fontSize)
        return;
    m_fontSize = clamped;
    SettingsStore::instance().setValue(QStringLiteral("terminal/fontSize"), m_fontSize);
    for (TerminalTab *tab : m_tabs) {
        if (TerminalView *view = tab->view())
            view->setFontSize(m_fontSize);
    }
}

TerminalView *TerminalPanel::currentView() const
{
    TerminalTab *tab = tabAt(m_tabBar->currentIndex());
    return tab ? tab->view() : nullptr;
}

QString TerminalPanel::currentTitle() const
{
    TerminalTab *tab = tabAt(m_tabBar->currentIndex());
    return tab ? tab->displayTitle() : QString();
}

QString TerminalPanel::shellProgram() const
{
    return m_shellProgram;
}

void TerminalPanel::setShellProgram(const QString &program)
{
    if (program.isEmpty() || program == m_shellProgram)
        return;
    m_shellProgram = program;
    SettingsStore::instance().setValue(QStringLiteral("terminal/shell"), m_shellProgram);
    populateShellCombo();
}

TerminalTab *TerminalPanel::createSession(const QString &program, const QStringList &args)
{
    QString shell = program.isEmpty() ? m_shellProgram : program;
    if (shell.isEmpty())
        shell = TerminalSession::defaultShell();

    QStringList arguments = args.isEmpty() ? shellArguments(shell) : args;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Shell integration: tell the child it is being driven from Scriptura so
    // prompt tooling (starship, zsh frames, direnv) can key off it.
    env.insert(QStringLiteral("TERM_PROGRAM"), QStringLiteral("Scriptura"));

    auto *tab = new TerminalTab(shell, arguments, m_workingDirectory, env, m_stack);
    m_tabs.append(tab);

    const int index = m_tabs.size() - 1;
    // The view goes into the stack first: inserting the very first tab makes the
    // tab bar emit currentChanged(), and onTabChanged() needs the view to be
    // reachable already.
    m_stack->addWidget(tab->view());
    m_tabBar->insertTab(index, ThemeIcons::instance()->statefulIcon(
                                         QStringLiteral(":/icons/console.svg"), 14), QString());
    m_tabBar->setCurrentIndex(index);

    applyViewSettings(tab);

    connect(tab, &TerminalTab::titleChanged, this,
            [this, tab]() { updateTabTitle(m_tabs.indexOf(tab)); });
    connect(tab, &TerminalTab::finished, this, [this, tab]() {
        const int index = m_tabs.indexOf(tab);
        if (index >= 0)
            updateTabTitle(index);
        emit sessionTitleChanged(currentTitle());
    });

    updateTabTitle(index);
    updateActionStates();
    emit sessionCountChanged(m_tabs.size());
    emit sessionTitleChanged(currentTitle());

    tab->view()->setFocus();
    return tab;
}

void TerminalPanel::closeSession(int index)
{
    TerminalTab *tab = tabAt(index);
    if (!tab)
        return;

    tab->shutdown();
    m_tabs.removeAt(index);
    m_tabBar->removeTab(index);
    // The view is destroyed here rather than left to the tab: the stack has just
    // given up ownership of it, and the tab is only scheduled for deletion, so
    // nothing may hold a pointer to it afterwards.
    TerminalView *view = tab->view();
    m_stack->removeWidget(view);
    tab->releaseView();
    delete view;
    tab->deleteLater();

    if (m_tabs.isEmpty()) {
        m_tabBar->setCurrentIndex(-1);
        m_stack->setCurrentIndex(-1);
    } else {
        m_tabBar->setCurrentIndex(qMin(index, m_tabs.size() - 1));
    }

    updateActionStates();
    emit sessionCountChanged(m_tabs.size());
    emit sessionTitleChanged(currentTitle());
}

void TerminalPanel::closeCurrentSession()
{
    closeSession(m_tabBar->currentIndex());
}

bool TerminalPanel::runCommand(const QString &command, bool sendNewline)
{
    if (command.isEmpty())
        return false;

    TerminalTab *tab = tabAt(m_tabBar->currentIndex());
    if (!tab || tab->isFinished())
        tab = createSession();
    if (!tab)
        return false;

    TerminalView *view = tab->view();
    if (!view)
        return false;

    // Type it first so the shell's readline shows the command being run and
    // any prompt-state errors (a running job, no write permission) are visible.
    sendText(command);
    if (sendNewline)
        tab->session()->write(QByteArray("\r"));
    view->setFocus();
    return true;
}

void TerminalPanel::sendText(const QString &text)
{
    if (text.isEmpty())
        return;
    TerminalTab *tab = tabAt(m_tabBar->currentIndex());
    if (tab && tab->session())
        tab->session()->write(text.toUtf8());
}

void TerminalPanel::focusTerminal()
{
    if (m_tabs.isEmpty())
        createSession();
    if (TerminalView *view = currentView())
        view->setFocus();
}

TerminalTab *TerminalPanel::tabAt(int index) const
{
    if (index < 0 || index >= m_tabs.size())
        return nullptr;
    return m_tabs.at(index);
}

void TerminalPanel::applyViewSettings(TerminalTab *tab)
{
    if (!tab)
        return;
    if (TerminalEmulator *emulator = tab->emulator()) {
        emulator->setDefaultForeground(m_foreground);
        emulator->setDefaultBackground(m_background);
    }
    if (TerminalView *view = tab->view()) {
        view->setFontSize(m_fontSize);
        view->applyStyle();
        connect(view, &TerminalView::requestClearScrollback, view, &TerminalView::clearScrollback);
        connect(view, &TerminalView::requestReset, this, [this, tab]() {
            if (TerminalView *view = tab->view())
                view->resetBuffer();
            // Tell the child it lost its screen, the way a real terminal's
            // "Reset" button does, so full-screen programs redraw.
            if (tab->session() && tab->session()->isRunning())
                tab->session()->write(QByteArray("\x1b" "c"));
        });
    }
}

void TerminalPanel::refreshTab(int index)
{
    TerminalTab *tab = tabAt(index);
    if (!tab)
        return;
    if (TerminalView *view = tab->view())
        view->viewport()->update();
}

void TerminalPanel::updateTabTitle(int index)
{
    TerminalTab *tab = tabAt(index);
    if (!tab || index < 0 || index >= m_tabBar->count())
        return;

    QString title = tab->displayTitle();
    if (tab->isFinished() && tab->session() && tab->session()->hasExited())
        title += tr(" — exited");
    m_tabBar->setTabText(index, title);
    m_tabBar->setTabToolTip(index, tab->statusText());

    if (index == m_tabBar->currentIndex()) {
        emit sessionTitleChanged(title);
        updateActionStates();
    }
}

void TerminalPanel::updateActionStates()
{
    TerminalTab *tab = tabAt(m_tabBar->currentIndex());
    const bool running = tab && tab->session() && tab->session()->isRunning();
    m_terminateButton->setEnabled(running);
    m_zoomInButton->setEnabled(true);
    m_zoomOutButton->setEnabled(m_fontSize > 6);
}

void TerminalPanel::onNewSessionClicked()
{
    createSession();
}

void TerminalPanel::onTabChanged(int index)
{
    TerminalTab *tab = tabAt(index);
    if (tab) {
        if (TerminalView *view = tab->view()) {
            if (m_stack->indexOf(view) >= 0)
                m_stack->setCurrentWidget(view);
            view->setFocus();
        }
        emit sessionTitleChanged(tab->displayTitle());
    }
    updateActionStates();
}

void TerminalPanel::onTabCloseRequested(int index)
{
    closeSession(index);
}

void TerminalPanel::onShellChosen(int index)
{
    if (index < 0)
        return;
    const QString program = m_shellCombo->itemData(index).toString();
    if (program.isEmpty())
        return;
    setShellProgram(program);
    createSession(program);
}

void TerminalPanel::onTerminateClicked()
{
    TerminalTab *tab = tabAt(m_tabBar->currentIndex());
    if (!tab || !tab->session())
        return;
    tab->shutdown();
    updateActionStates();
}

void TerminalPanel::onZoomInClicked()
{
    TerminalView *view = currentView();
    if (!view)
        return;
    view->increaseFontSize();
    setFontSize(view->fontSize());
}

void TerminalPanel::onZoomOutClicked()
{
    TerminalView *view = currentView();
    if (!view)
        return;
    view->decreaseFontSize();
    setFontSize(view->fontSize());
}