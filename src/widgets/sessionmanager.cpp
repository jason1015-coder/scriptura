#include "sessionmanager.h"
#include "codeeditor.h"
#include <QMainWindow>
#include <QTabWidget>
#include <QSettings>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QTextCursor>
#include <QTextDocument>

SessionManager::SessionManager(QMainWindow *mainWindow, QTabWidget *tabWidget, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_tabWidget(tabWidget)
{
}

QString SessionManager::sessionFilePath() const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    return dataDir + "/session.json";
}

QString SessionManager::hotExitDir() const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString dir = dataDir + "/hotexit";
    QDir().mkpath(dir);
    return dir;
}

bool SessionManager::hasSavedSession() const
{
    return QFile::exists(sessionFilePath());
}

void SessionManager::clearSession()
{
    QFile::remove(sessionFilePath());
    QDir hotDir(hotExitDir());
    if (hotDir.exists()) {
        hotDir.removeRecursively();
    }
}

void SessionManager::saveSession()
{
    if (!m_autoSaveSession || !m_tabWidget) return;

    QJsonObject session;
    session["version"] = 1;

    // Save window geometry and state
    if (m_mainWindow) {
        session["geometry"] = QString(m_mainWindow->saveGeometry().toBase64());
        session["windowState"] = QString(m_mainWindow->saveState().toBase64());
    }

    // Save tab states
    QJsonArray tabs;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        QJsonObject tabData = serializeEditorState(i);
        if (!tabData.isEmpty()) {
            tabs.append(tabData);
        }
    }
    session["tabs"] = tabs;
    session["activeTab"] = m_tabWidget->currentIndex();

    // Save session
    QFile file(sessionFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(session).toJson());
        file.close();
    }

    // Save hot exit data for unsaved files
    if (m_hotExitEnabled) {
        saveUnsavedBuffers();
    }

    emit sessionSaved();
}

QJsonObject SessionManager::serializeEditorState(int tabIndex)
{
    QWidget *widget = m_tabWidget->widget(tabIndex);
    CodeEditor *editor = qobject_cast<CodeEditor*>(widget);
    if (!editor) return QJsonObject();

    QJsonObject state;
    state["filePath"] = editor->filePath();
    state["cursorLine"] = editor->textCursor().blockNumber();
    state["cursorColumn"] = editor->textCursor().positionInBlock();
    state["modified"] = editor->document()->isModified();

    return state;
}

bool SessionManager::restoreSession()
{
    QFile file(sessionFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QJsonObject session = doc.object();
    if (session["version"].toInt() != 1)
        return false;

    // Restore window geometry
    if (m_mainWindow && session.contains("geometry")) {
        m_mainWindow->restoreGeometry(QByteArray::fromBase64(session["geometry"].toString().toUtf8()));
    }
    if (m_mainWindow && session.contains("windowState")) {
        m_mainWindow->restoreState(QByteArray::fromBase64(session["windowState"].toString().toUtf8()));
    }

    // Restore tabs by emitting file paths for the main window to open
    QJsonArray tabs = session["tabs"].toArray();
    int activeTab = session["activeTab"].toInt(0);

    // Emit individual file paths so MainWindow can open them
    for (int i = 0; i < tabs.size(); ++i) {
        QJsonObject tabData = tabs[i].toObject();
        QString filePath = tabData["filePath"].toString();
        if (!filePath.isEmpty() && QFile::exists(filePath)) {
            // Store cursor position as a property for MainWindow to restore
            // We use a signal with the full tab data
            emit sessionFileRequested(filePath,
                                      tabData["cursorLine"].toInt(),
                                      tabData["cursorColumn"].toInt(),
                                      i == activeTab);
        }
    }

    // Restore hot exit buffers (unsaved files)
    restoreUnsavedBuffers();

    emit sessionRestored();
    return true;
}

void SessionManager::saveUnsavedBuffers()
{
    if (!m_tabWidget) return;

    QDir hotDir(hotExitDir());
    hotDir.removeRecursively();
    QDir().mkpath(hotExitDir());

    QJsonArray unsavedFiles;

    for (int i = 0; i < m_tabWidget->count(); ++i) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(m_tabWidget->widget(i));
        if (!editor) continue;

        if (editor->document()->isModified() || editor->filePath().isEmpty()) {
            QString identifier = editor->filePath().isEmpty()
                ? QString("untitled_%1").arg(i)
                : QString::number(qHash(editor->filePath()));

            QString hotFilePath = hotExitDir() + "/" + identifier + ".json";

            QJsonObject hotData;
            hotData["originalPath"] = editor->filePath();
            hotData["content"] = editor->toPlainText();
            hotData["cursorLine"] = editor->textCursor().blockNumber();
            hotData["cursorColumn"] = editor->textCursor().positionInBlock();
            hotData["modified"] = true;

            QFile hotFile(hotFilePath);
            if (hotFile.open(QIODevice::WriteOnly)) {
                hotFile.write(QJsonDocument(hotData).toJson());
                hotFile.close();
            }

            unsavedFiles.append(hotData);
        }
    }

    // Save index of unsaved files
    QFile indexFile(hotExitDir() + "/index.json");
    if (indexFile.open(QIODevice::WriteOnly)) {
        QJsonObject index;
        index["files"] = unsavedFiles;
        indexFile.write(QJsonDocument(index).toJson());
        indexFile.close();
    }
}

bool SessionManager::restoreUnsavedBuffers()
{
    QFile indexFile(hotExitDir() + "/index.json");
    if (!indexFile.open(QIODevice::ReadOnly))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(indexFile.readAll());
    indexFile.close();

    QJsonObject index = doc.object();
    QJsonArray files = index["files"].toArray();

    for (const QJsonValue &v : files) {
        QJsonObject fileData = v.toObject();
        QString content = fileData["content"].toString();
        QString originalPath = fileData["originalPath"].toString();
        int cursorLine = fileData["cursorLine"].toInt();
        int cursorColumn = fileData["cursorColumn"].toInt();

        // Emit signal so MainWindow can create untitled tabs with restored content
        emit hotExitFileRequested(originalPath, content, cursorLine, cursorColumn);
    }

    return !files.isEmpty();
}
