#include "terminalpanel.h"

#include <QApplication>
#include <QKeyEvent>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QColor>
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>

TerminalPanel::TerminalPanel(QWidget *parent)
    : QWidget(parent)
    , m_outputEdit(new QPlainTextEdit(this))
    , m_inputEdit(new QLineEdit(this))
    , m_clearButton(new QPushButton(tr("Clear"), this))
    , m_statusLabel(new QLabel(tr("Ready"), this))
    , m_mainLayout(new QVBoxLayout(this))
    , m_process(new QProcess(this))
    , m_processRunning(false)
    , m_historyIndex(-1)
{
    setObjectName("terminalPanel");

    // Output area
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setPlaceholderText(tr("Terminal output will appear here..."));
    m_outputEdit->setStyleSheet(
        "QPlainTextEdit { border: none; background: palette(base); font-family: monospace; }"
    );
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_outputEdit->setFont(monoFont);

    // Input area
    QWidget *inputWidget = new QWidget(this);
    QHBoxLayout *inputLayout = new QHBoxLayout(inputWidget);
    inputLayout->setContentsMargins(4, 4, 4, 4);
    inputLayout->setSpacing(4);

    QLabel *promptLabel = new QLabel("$", inputWidget);
    promptLabel->setStyleSheet("font-family: monospace; font-weight: bold;");
    inputLayout->addWidget(promptLabel);

    m_inputEdit->setStyleSheet("QLineEdit { border: none; background: palette(base); font-family: monospace; }");
    m_inputEdit->setFont(monoFont);
    inputLayout->addWidget(m_inputEdit);

    m_clearButton->setFixedSize(60, 24);
    m_clearButton->setToolTip(tr("Clear terminal"));
    inputLayout->addWidget(m_clearButton);

    // Status bar
    QWidget *statusWidget = new QWidget(this);
    QHBoxLayout *statusLayout = new QHBoxLayout(statusWidget);
    statusLayout->setContentsMargins(4, 2, 4, 2);
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();

    m_mainLayout->setContentsMargins(4, 4, 4, 4);
    m_mainLayout->setSpacing(0);
    m_mainLayout->addWidget(m_outputEdit);
    m_mainLayout->addWidget(inputWidget);
    m_mainLayout->addWidget(statusWidget);

    // Connections
    connect(m_inputEdit, &QLineEdit::returnPressed, this, &TerminalPanel::onReturnPressed);
    connect(m_clearButton, &QPushButton::clicked, this, &TerminalPanel::onClearClicked);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &TerminalPanel::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &TerminalPanel::onReadyReadStandardError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TerminalPanel::onProcessFinished);
    connect(m_process, QOverload<QProcess::ProcessError>::of(&QProcess::errorOccurred),
            this, &TerminalPanel::onProcessError);
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

    // Show command in output
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

void TerminalPanel::appendOutput(const QString &text, bool isError)
{
    QString processed = text;
    processAnsiCodes(processed);

    QTextCursor cursor = m_outputEdit->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat format;
    if (isError) {
        format.setForeground(QColor("#ff5555"));
    }

    cursor.insertText(processed, format);
    m_outputEdit->setTextCursor(cursor);
    m_outputEdit->ensureCursorVisible();
}

void TerminalPanel::processAnsiCodes(QString &text)
{
    // Basic ANSI escape code processing
    // Handle common color codes: \033[1;31m (bold red), \033[0m (reset), etc.
    static QRegularExpression ansiRegex("\033\\[([0-9;]+)m");
    QRegularExpressionMatchIterator it = ansiRegex.globalMatch(text);

    QString result;
    int lastEnd = 0;
    QTextCharFormat currentFormat;

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int start = match.capturedStart();
        int end = match.capturedEnd();

        // Add text before this escape code
        result.append(text.mid(lastEnd, start - lastEnd));

        // Parse the ANSI code
        QString codes = match.captured(1);
        QStringList codeList = codes.split(";", Qt::SkipEmptyParts);

        for (const QString &code : codeList) {
            int value = code.toInt();
            switch (value) {
            case 0: // Reset
                currentFormat = QTextCharFormat();
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
            case 37: currentFormat.setForeground(QColor("#e5e5e5")); break;
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
    result.append(text.mid(lastEnd));
    text = result;
}
