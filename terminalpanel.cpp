#include "terminalpanel.h"

#include <QApplication>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QColor>
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>
#include <QStyle>
#include <QFile>

TerminalPanel::TerminalPanel(QWidget *parent)
    : QWidget(parent)
    , m_outputEdit(new QPlainTextEdit(this))
    , m_inputEdit(new QLineEdit(this))
    , m_clearButton(new QPushButton(tr("Clear"), this))
    , m_titleLabel(new QLabel(tr("Terminal"), this))
    , m_statusLabel(new QLabel(tr("Ready"), this))
    , m_mainLayout(new QVBoxLayout(this))
    , m_contentLayout(new QVBoxLayout())
    , m_inputLayout(new QHBoxLayout())
    , m_statusLayout(new QHBoxLayout())
    , m_process(new QProcess(this))
    , m_processRunning(false)
    , m_historyIndex(-1)
{
    setObjectName("terminalPanel");

    // Main layout setup
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Title bar
    QWidget *titleBar = new QWidget(this);
    titleBar->setObjectName("terminalTitleBar");
    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(8, 4, 8, 4);
    titleLayout->setSpacing(8);

    m_titleLabel->setObjectName("terminalTitleLabel");
    m_titleLabel->setStyleSheet("font-weight: bold; color: #ffffff;");
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();

    m_clearButton->setFixedSize(60, 24);
    m_clearButton->setToolTip(tr("Clear terminal (Ctrl+L)"));
    titleLayout->addWidget(m_clearButton);

    m_mainLayout->addWidget(titleBar);

    // Output area
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setPlaceholderText(tr("Terminal output will appear here..."));
    m_outputEdit->setStyleSheet(
        "QPlainTextEdit { border: none; background: #1e1e1e; color: #d4d4d4; font-family: monospace; }"
    );
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(10);
    m_outputEdit->setFont(monoFont);
    m_outputEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_outputEdit->setMinimumHeight(150);

    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);
    m_contentLayout->addWidget(m_outputEdit);
    m_mainLayout->addLayout(m_contentLayout);

    // Input area
    QWidget *inputWidget = new QWidget(this);
    inputWidget->setObjectName("terminalInputWidget");
    m_inputLayout->setContentsMargins(4, 4, 4, 4);
    m_inputLayout->setSpacing(4);

    m_promptLabel = new QLabel("$", inputWidget);
    m_promptLabel->setObjectName("terminalPromptLabel");
    m_promptLabel->setStyleSheet("font-family: monospace; font-weight: bold; color: #4ec9b0; background: transparent;");
    m_inputLayout->addWidget(m_promptLabel);

    m_inputEdit->setStyleSheet("QLineEdit { border: none; background: #1e1e1e; color: #d4d4d4; font-family: monospace; }");
    m_inputEdit->setFont(monoFont);
    m_inputLayout->addWidget(m_inputEdit);

    inputWidget->setLayout(m_inputLayout);
    m_mainLayout->addWidget(inputWidget);

    // Status bar
    QWidget *statusWidget = new QWidget(this);
    statusWidget->setObjectName("terminalStatusWidget");
    m_statusLayout->setContentsMargins(8, 2, 8, 2);
    m_statusLayout->addWidget(m_statusLabel);
    m_statusLayout->addStretch();
    statusWidget->setLayout(m_statusLayout);

    m_statusLabel->setStyleSheet("color: #888888; font-size: 11px;");
    m_mainLayout->addWidget(statusWidget);

    // Install event filter for keyboard shortcuts
    m_inputEdit->installEventFilter(this);

    // Connections
    connect(m_inputEdit, &QLineEdit::returnPressed, this, &TerminalPanel::onReturnPressed);
    connect(m_clearButton, &QPushButton::clicked, this, &TerminalPanel::onClearClicked);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &TerminalPanel::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &TerminalPanel::onReadyReadStandardError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TerminalPanel::onProcessFinished);
    connect(m_process, QOverload<QProcess::ProcessError>::of(&QProcess::errorOccurred),
            this, &TerminalPanel::onProcessError);

    // Style the title bar and status bar
    titleBar->setStyleSheet("background: #2d2d2d; border-bottom: 1px solid #3c3c3c;");
    statusWidget->setStyleSheet("background: #252526; border-top: 1px solid #3c3c3c;");
}

TerminalPanel::~TerminalPanel()
{
    stopShell();
}

void TerminalPanel::startShell(const QString &workingDir)
{
    if (m_processRunning)
        stopShell();

    m_workingDir = workingDir.isEmpty() ? QDir::currentPath() : workingDir;
    QString shell = findShell();

    m_process->setWorkingDirectory(m_workingDir);
    m_process->start(shell, QStringList() << "-i");

    if (!m_process->waitForStarted(3000)) {
        m_statusLabel->setText(tr("Failed to start shell: %1").arg(shell));
        return;
    }

    m_processRunning = true;
    m_statusLabel->setText(tr("Shell: %1").arg(shell));
    updateTitle();
    updatePrompt();
    m_inputEdit->setFocus();
}

void TerminalPanel::stopShell()
{
    if (m_processRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(1000))
            m_process->kill();
        m_processRunning = false;
        m_statusLabel->setText(tr("Shell stopped"));
    }
}

bool TerminalPanel::isRunning() const
{
    return m_processRunning && m_process->state() == QProcess::Running;
}

void TerminalPanel::onReadyReadStandardOutput()
{
    QString text = QString::fromLocal8Bit(m_process->readAllStandardOutput());
    appendOutput(text, false);
}

void TerminalPanel::onReadyReadStandardError()
{
    QString text = QString::fromLocal8Bit(m_process->readAllStandardError());
    appendOutput(text, true);
}

void TerminalPanel::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)
    m_processRunning = false;
    m_statusLabel->setText(tr("Shell exited"));
}

void TerminalPanel::onProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error)
    m_statusLabel->setText(tr("Shell error: %1").arg(m_process->errorString()));
}

void TerminalPanel::onReturnPressed()
{
    QString command = m_inputEdit->text();
    if (command.isEmpty())
        return;

    // Add to history
    m_commandHistory.prepend(command);
    if (m_commandHistory.size() > 100)
        m_commandHistory.removeLast();
    m_historyIndex = -1;

    // Show command in output with $ prefix to match the prompt
    appendOutput("$ " + command + "\n", false);

    // Send to process
    QByteArray data = (command + "\n").toLocal8Bit();
    m_process->write(data);

    m_inputEdit->clear();
    emit commandExecuted(command);
}

void TerminalPanel::onClearClicked()
{
    m_outputEdit->clear();
}

void TerminalPanel::onWorkingDirChanged(const QString &dir)
{
    m_workingDir = dir;
    if (m_processRunning) {
        // Restart shell in new directory
        startShell(m_workingDir);
    }
    updatePrompt();
}

void TerminalPanel::setWorkingDirectory(const QString &dir)
{
    m_workingDir = dir;
    updatePrompt();
    updateTitle();
}

QString TerminalPanel::findShell() const
{
#ifdef Q_OS_WIN
    return "cmd.exe";
#elif defined(Q_OS_MAC)
    return "/bin/zsh";
#else
    // Try common shells in order of preference
    QStringList shells = {"/bin/bash", "/bin/zsh", "/bin/sh"};
    for (const QString &shell : shells) {
        if (QFile::exists(shell))
            return shell;
    }
    return "/bin/sh";
#endif
}

void TerminalPanel::updateTitle()
{
    QString shellName = m_process->program();
    QFileInfo shellInfo(shellName);
    QString dirName = QFileInfo(m_workingDir).fileName();
    if (dirName.isEmpty())
        dirName = m_workingDir;
    m_titleLabel->setText(tr("Terminal - %1").arg(dirName));
}

void TerminalPanel::updatePrompt()
{
    // Just show a simple $ prompt - the shell will output its own prompt with path
    m_promptLabel->setText("$");
}

bool TerminalPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_inputEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        
        // Ctrl+L to clear terminal
        if (keyEvent->key() == Qt::Key_L && keyEvent->modifiers() & Qt::ControlModifier) {
            onClearClicked();
            return true;
        }
        
        // Up arrow for history
        if (keyEvent->key() == Qt::Key_Up && !m_commandHistory.isEmpty()) {
            if (m_historyIndex == -1) {
                m_historyIndex = 0;
            } else if (m_historyIndex < m_commandHistory.size() - 1) {
                m_historyIndex++;
            }
            m_inputEdit->setText(m_commandHistory[m_historyIndex]);
            m_inputEdit->setCursorPosition(m_inputEdit->text().length());
            return true;
        }
        
        // Down arrow for history
        if (keyEvent->key() == Qt::Key_Down && m_historyIndex >= 0) {
            if (m_historyIndex > 0) {
                m_historyIndex--;
                m_inputEdit->setText(m_commandHistory[m_historyIndex]);
            } else {
                m_historyIndex = -1;
                m_inputEdit->clear();
            }
            m_inputEdit->setCursorPosition(m_inputEdit->text().length());
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void TerminalPanel::appendOutput(const QString &text, bool isError)
{
    QTextCursor cursor = m_outputEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    processAnsiCodes(text, isError, cursor);
    m_outputEdit->setTextCursor(cursor);
    m_outputEdit->ensureCursorVisible();
}

void TerminalPanel::processAnsiCodes(const QString &text, bool isError, QTextCursor &cursor)
{
    // Basic ANSI escape code processing
    // Handle common color codes: \033[1;31m (bold red), \033[0m (reset), etc.
    static QRegularExpression ansiRegex("\033\\[([0-9;]+)m");
    QRegularExpressionMatchIterator it = ansiRegex.globalMatch(text);

    int lastEnd = 0;
    QTextCharFormat currentFormat;
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(10);
    currentFormat.setFont(monoFont);

    if (isError) {
        currentFormat.setForeground(QColor("#ff5555"));
    }

    if (!it.hasNext()) {
        // No ANSI codes, just insert the text
        cursor.insertText(text, currentFormat);
        return;
    }

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int start = match.capturedStart();
        int end = match.capturedEnd();

        // Add text before this escape code
        if (start > lastEnd) {
            cursor.insertText(text.mid(lastEnd, start - lastEnd), currentFormat);
        }

        // Parse the ANSI code
        QString codes = match.captured(1);
        QStringList codeList = codes.split(";", Qt::SkipEmptyParts);

        for (const QString &code : codeList) {
            int value = code.toInt();
            switch (value) {
            case 0: // Reset
                currentFormat = QTextCharFormat();
                currentFormat.setFont(monoFont);
                break;
            case 1: // Bold
                currentFormat.setFontWeight(QFont::Bold);
                break;
            case 30: currentFormat.setForeground(QColor("#000000")); break;
            case 31: currentFormat.setForeground(QColor("#cd3131")); break;
            case 32: currentFormat.setForeground(QColor("#0dbc79")); break;
            case 33: currentFormat.setForeground(QColor("#e5e510")); break;
            case 34: currentFormat.setForeground(QColor("#2472c8")); break;
            case 35: currentFormat.setForeground(QColor("#bc3fbc")); break;
            case 36: currentFormat.setForeground(QColor("#11a8cd")); break;
            case 37: currentFormat.setForeground(QColor("#d4d4d4")); break;
            case 90: currentFormat.setForeground(QColor("#666666")); break;
            case 91: currentFormat.setForeground(QColor("#f14c4c")); break;
            case 92: currentFormat.setForeground(QColor("#23d18b")); break;
            case 93: currentFormat.setForeground(QColor("#f5f543")); break;
            case 94: currentFormat.setForeground(QColor("#3b8eea")); break;
            case 95: currentFormat.setForeground(QColor("#d670d6")); break;
            case 96: currentFormat.setForeground(QColor("#29b8db")); break;
            case 97: currentFormat.setForeground(QColor("#ffffff")); break;
            }
        }

        lastEnd = end;
    }

    // Add remaining text
    if (lastEnd < text.length()) {
        cursor.insertText(text.mid(lastEnd), currentFormat);
    }
}
