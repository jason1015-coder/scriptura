#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "codeeditor.h"
#include "rust_adapter.h"
#include "debugconfiguration.h"
#include "rundialog.h"

#include <QMessageBox>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileDialog>
#include <QDebug>
#include <QInputDialog>
#include <QStandardPaths>
#include "version.h"

void MainWindow::on_action_show_document_symbols_triggered()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor || currentFile.isEmpty())
        return;
    QString uri = QUrl::fromLocalFile(currentFile).toString();
    lspClient->documentSymbol(uri);
    qDebug() << "Document symbols requested for" << currentFile;
}

// Debug methods
void MainWindow::on_action_run_debug_triggered()
{
    loadDebugConfigurations();
    
    RunDialog dialog(this);
    dialog.setConfigurations(debugConfigManager->configurations());
    
    if (dialog.exec() == QDialog::Accepted) {
        QList<DebugConfiguration> configs = dialog.configurations();
        DebugConfiguration selected = dialog.selectedConfiguration();
        QString mode = dialog.selectedMode();

        if (!projectDir.isEmpty()) {
            QString configPath = projectDir + "/.vscode/launch.json";
            QDir().mkpath(projectDir + "/.vscode");
            debugConfigManager->setConfigurations(configs);
            debugConfigManager->saveToFile(configPath);
        }

        if (mode == "debug") {
            startDebug(selected.name);
        } else {
            if (selected.program.isEmpty()) {
                QMessageBox::warning(this, tr("Run"),
                    tr("No program specified for configuration \"%1\".").arg(selected.name));
                return;
            }
            bool started = QProcess::startDetached(
                selected.program, selected.args, selected.cwd.isEmpty() ? QDir::currentPath() : selected.cwd);
            if (!started)
                QMessageBox::warning(this, tr("Run"),
                    tr("Failed to start program: %1").arg(selected.program));
        }
    }
}

void MainWindow::on_action_stop_debug_triggered()
{
    if (m_isDebugging) {
        stopDebug();
    }
}

void MainWindow::on_action_step_over_triggered()
{
    if (m_isDebugging && dapClient->isRunning()) {
        dapClient->next();
    }
}

void MainWindow::on_action_step_into_triggered()
{
    if (m_isDebugging && dapClient->isRunning()) {
        dapClient->stepIn();
    }
}

void MainWindow::on_action_step_out_triggered()
{
    if (m_isDebugging && dapClient->isRunning()) {
        dapClient->stepOut();
    }
}

void MainWindow::on_action_continue_debug_triggered()
{
    if (m_isDebugging && dapClient->isRunning()) {
        dapClient->continueDebug();
    }
}

void MainWindow::on_action_toggle_breakpoint_triggered()
{
    CodeEditor *editor = getCurrentCodeEditor();
    if (!editor)
        return;
    
    QTextCursor cursor = editor->textCursor();
    int line = cursor.blockNumber() + 1;
    QString key = currentFile + ":" + QString::number(line);
    
    bool enabled = editor->breakpointLines().contains(line);
    if (enabled) {
        editor->setBreakpointLine(line, false);
        m_breakpointConditions.remove(key);
    } else {
        bool ok = false;
        QString condition = QInputDialog::getText(this, tr("Conditional Breakpoint"),
            tr("Breakpoint condition (leave empty to break always):"), QLineEdit::Normal, QString(), &ok);
        if (!ok)
            return;
        editor->setBreakpointLine(line, true);
        if (!condition.isEmpty())
            m_breakpointConditions[key] = condition;
    }
    
    if (m_isDebugging && dapClient->isRunning()) {
        updateDapBreakpoints();
    }
}

void MainWindow::updateDapBreakpoints()
{
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(ui->tabWidget->widget(i));
        if (!editor)
            continue;
        QString filePath = (i < openFiles.size()) ? openFiles[i].filePath : QString();
        if (filePath.isEmpty())
            continue;
        QList<int> lines = editor->breakpointLines().values();
        dapClient->setBreakpoints(filePath, lines);
    }
}

void MainWindow::startDebug(const QString &configName)
{
    DebugConfiguration config = debugConfigManager->configuration(configName);
    if (config.name.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Debug configuration not found: %1").arg(configName));
        return;
    }
    
    QString debuggerPath = config.debuggerPath;
    if (debuggerPath.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("No debugger path specified in configuration."));
        return;
    }
    
    if (!dapClient->startServer(debuggerPath, QStringList())) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to start debugger adapter."));
        return;
    }
    
    dapClient->initialize(config.program, config.args, config.cwd);
    
    m_isDebugging = true;
    
}

void MainWindow::stopDebug()
{
    if (dapClient->isRunning()) {
        dapClient->disconnect();
        dapClient->stopServer();
    }
    
    m_isDebugging = false;
    
    CodeEditor *editor = getCurrentCodeEditor();
    if (editor) {
        editor->clearBreakpoints();
    }
}

void MainWindow::loadDebugConfigurations()
{
    QString configPath = projectDir + "/.vscode/launch.json";
    if (QFile::exists(configPath)) {
        debugConfigManager->loadFromFile(configPath);
    } else {
        DebugConfiguration defaultConfig;
        defaultConfig.name = "Default";
        defaultConfig.type = "cppdbg";
        defaultConfig.request = "launch";
        defaultConfig.program = "";
        defaultConfig.args = QStringList();
        defaultConfig.cwd = projectDir;
        debugConfigManager->addConfiguration(defaultConfig);
    }
}

void MainWindow::onBreakpointToggled(int line, bool enabled)
{
    Q_UNUSED(line)
    Q_UNUSED(enabled)
}

void MainWindow::onDapInitialized()
{
    qDebug() << "DAP initialized";
    updateDapBreakpoints();
    dapClient->launch();
}

void MainWindow::onDapStopped(const QString &reason)
{
    qDebug() << "DAP stopped:" << reason;
    
    dapClient->stackTrace(1); // Default thread ID 1
}

void MainWindow::onDapContinued()
{
    qDebug() << "DAP continued";
    CodeEditor *editor = getCurrentCodeEditor();
    if (editor) {
        editor->highlightCurrentLine(-1);
    }
}

void MainWindow::onStackTraceReceived(int threadId, const QJsonArray &frames)
{
    Q_UNUSED(threadId)
    qDebug() << "Stack trace received with" << frames.size() << "frames";
    
    if (!frames.isEmpty()) {
        QJsonObject topFrame = frames[0].toObject();
        m_currentFrameId = topFrame["id"].toInt();
        QString sourcePath = topFrame["source"].toObject()["path"].toString();
        int line = topFrame["line"].toInt();
        
        CodeEditor *editor = getCurrentCodeEditor();
        if (editor && sourcePath == currentFile) {
            editor->highlightCurrentLine(line);
        }

        dapClient->scopes(m_currentFrameId);
    }
}

void MainWindow::onScopesReceived(int frameId, const QJsonArray &scopes)
{
    Q_UNUSED(frameId)
    qDebug() << "Scopes received:" << scopes.size();
    
    for (const QJsonValue &v : scopes) {
        QJsonObject scope = v.toObject();
        int varRef = scope["variablesReference"].toInt();
        if (varRef > 0) {
            dapClient->variables(varRef);
        }
    }
}

void MainWindow::onVariablesReceived(int variablesReference, const QJsonArray &variables)
{
    Q_UNUSED(variablesReference)
    Q_UNUSED(variables)
}

void MainWindow::onDapLogMessage(const QString &msg)
{
    qDebug() << "DAP:" << msg;
}
