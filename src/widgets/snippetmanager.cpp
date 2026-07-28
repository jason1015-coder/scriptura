#include "snippetmanager.h"
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QApplication>

SnippetManager::SnippetManager(QObject *parent)
    : QObject(parent)
    , m_currentTabStopIndex(-1)
{
    loadFromSettings();
}

void SnippetManager::addSnippet(const Snippet &snippet)
{
    // Check for duplicate ID
    for (const Snippet &s : m_snippets) {
        if (s.id == snippet.id) {
            return;
        }
    }
    
    m_snippets.append(snippet);
    saveToSettings();
    emit snippetAdded(snippet.id);
    emit snippetsChanged();
}

void SnippetManager::updateSnippet(const Snippet &snippet)
{
    for (int i = 0; i < m_snippets.size(); ++i) {
        if (m_snippets[i].id == snippet.id) {
            m_snippets[i] = snippet;
            saveToSettings();
            emit snippetsChanged();
            return;
        }
    }
}

void SnippetManager::removeSnippet(const QString &id)
{
    for (int i = 0; i < m_snippets.size(); ++i) {
        if (m_snippets[i].id == id) {
            m_snippets.removeAt(i);
            saveToSettings();
            emit snippetRemoved(id);
            emit snippetsChanged();
            return;
        }
    }
}

Snippet SnippetManager::snippetById(const QString &id) const
{
    for (const Snippet &s : m_snippets) {
        if (s.id == id) {
            return s;
        }
    }
    return Snippet();
}

QList<Snippet> SnippetManager::snippetsForLanguage(const QString &language) const
{
    QList<Snippet> result;
    for (const Snippet &s : m_snippets) {
        if (s.language.isEmpty() || s.language == language || s.language == "*") {
            result.append(s);
        }
    }
    return result;
}

QStringList SnippetManager::snippetPrefixes() const
{
    QStringList prefixes;
    for (const Snippet &s : m_snippets) {
        if (!s.prefix.isEmpty()) {
            prefixes.append(s.prefix);
        }
    }
    return prefixes;
}

void SnippetManager::insertSnippet(QPlainTextEdit *editor, const Snippet &snippet)
{
    if (!editor) return;
    
    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();
    
    // Substitute variables
    QString body = substituteVariables(snippet.body);
    
    // Clear any existing tab stops
    clearTabStops();
    
    // Parse and insert the snippet
    QList<QPair<int, QString>> parsedStops;
    parseTabStops(body, parsedStops);
    
    // Convert to TabStop structs
    m_tabStops.clear();
    for (const auto &ps : parsedStops) {
        TabStop ts;
        ts.position = ps.first;
        ts.length = ps.second.isEmpty() ? 0 : ps.second.length();
        ts.placeholder = ps.second;
        m_tabStops.append(ts);
    }
    
    // Insert the body
    cursor.insertText(body);
    
    // Set up tab stops (convert from placeholder positions to document positions)
    int basePos = cursor.position() - body.length();
    for (int i = 0; i < m_tabStops.size(); ++i) {
        m_tabStops[i].position += basePos;
    }
    
    cursor.endEditBlock();
    editor->setTextCursor(cursor);
    
    // Jump to first tab stop
    if (!m_tabStops.isEmpty()) {
        m_currentTabStopIndex = 0;
        nextTabStop(editor);
    }
    
    emit snippetInserted(snippet.id);
}

bool SnippetManager::hasSnippetForPrefix(const QString &prefix, const QString &language) const
{
    for (const Snippet &s : m_snippets) {
        if (s.prefix == prefix && 
            (s.language.isEmpty() || s.language == language || s.language == "*")) {
            return true;
        }
    }
    return false;
}

Snippet SnippetManager::snippetForPrefix(const QString &prefix, const QString &language) const
{
    for (const Snippet &s : m_snippets) {
        if (s.prefix == prefix && 
            (s.language.isEmpty() || s.language == language || s.language == "*")) {
            return s;
        }
    }
    return Snippet();
}

void SnippetManager::nextTabStop(QPlainTextEdit *editor)
{
    if (!editor || m_tabStops.isEmpty()) return;
    
    // Move to next tab stop
    m_currentTabStopIndex++;
    
    if (m_currentTabStopIndex >= m_tabStops.size()) {
        // No more tab stops, clear and finish
        clearTabStops();
        return;
    }
    
    const TabStop &stop = m_tabStops[m_currentTabStopIndex];
    
    QTextCursor cursor = editor->textCursor();
    cursor.setPosition(stop.position);
    
    if (stop.length > 0) {
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, stop.length);
    }
    
    editor->setTextCursor(cursor);
}

void SnippetManager::previousTabStop(QPlainTextEdit *editor)
{
    if (!editor || m_tabStops.isEmpty()) return;
    
    // Move to previous tab stop
    m_currentTabStopIndex--;
    
    if (m_currentTabStopIndex < 0) {
        m_currentTabStopIndex = 0;
        return;
    }
    
    const TabStop &stop = m_tabStops[m_currentTabStopIndex];
    
    QTextCursor cursor = editor->textCursor();
    cursor.setPosition(stop.position);
    
    if (stop.length > 0) {
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, stop.length);
    }
    
    editor->setTextCursor(cursor);
}

void SnippetManager::clearTabStops()
{
    m_tabStops.clear();
    m_currentTabStopIndex = -1;
}

void SnippetManager::saveToSettings()
{
    QSettings settings;
    QJsonArray arr;
    
    for (const Snippet &s : m_snippets) {
        QJsonObject obj;
        obj["id"] = s.id;
        obj["name"] = s.name;
        obj["prefix"] = s.prefix;
        obj["body"] = s.body;
        obj["description"] = s.description;
        obj["language"] = s.language;
        obj["tabStops"] = s.tabStops;
        arr.append(obj);
    }
    
    settings.setValue("snippets", QJsonDocument(arr).toJson());
}

void SnippetManager::loadFromSettings()
{
    QSettings settings;
    QByteArray data = settings.value("snippets").toByteArray();
    
    if (data.isEmpty()) return;
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonArray arr = doc.array();
    
    m_snippets.clear();
    
    for (const QJsonValue &v : arr) {
        QJsonObject obj = v.toObject();
        Snippet s;
        s.id = obj["id"].toString();
        s.name = obj["name"].toString();
        s.prefix = obj["prefix"].toString();
        s.body = obj["body"].toString();
        s.description = obj["description"].toString();
        s.language = obj["language"].toString();
        s.tabStops = obj["tabStops"].toInt();
        m_snippets.append(s);
    }
}

bool SnippetManager::importSnippets(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        return false;
    }
    
    QJsonArray arr = doc.array();
    for (const QJsonValue &v : arr) {
        QJsonObject obj = v.toObject();
        Snippet s;
        s.id = obj["id"].toString();
        s.name = obj["name"].toString();
        s.prefix = obj["prefix"].toString();
        s.body = obj["body"].toString();
        s.description = obj["description"].toString();
        s.language = obj["language"].toString();
        s.tabStops = obj["tabStops"].toInt();
        
        // Check for duplicate
        bool exists = false;
        for (const Snippet &existing : m_snippets) {
            if (existing.id == s.id) {
                exists = true;
                break;
            }
        }
        
        if (!exists) {
            m_snippets.append(s);
        }
    }
    
    saveToSettings();
    emit snippetsChanged();
    return true;
}

bool SnippetManager::exportSnippets(const QString &filePath) const
{
    QJsonArray arr;
    
    for (const Snippet &s : m_snippets) {
        QJsonObject obj;
        obj["id"] = s.id;
        obj["name"] = s.name;
        obj["prefix"] = s.prefix;
        obj["body"] = s.body;
        obj["description"] = s.description;
        obj["language"] = s.language;
        obj["tabStops"] = s.tabStops;
        arr.append(obj);
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(QJsonDocument(arr).toJson());
    file.close();
    return true;
}

void SnippetManager::parseTabStops(const QString &body, QList<QPair<int, QString>> &stops) const
{
    stops.clear();
    
    QRegularExpression re("\\$(\\d+)(?::([^}]+))?|\\$\\{(\\d+)(?::([^}]+))?\\}");
    QRegularExpressionMatchIterator it = re.globalMatch(body);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        
        int index = match.captured(1).isEmpty() ? match.captured(3).toInt() : match.captured(1).toInt();
        QString placeholder = match.captured(2).isEmpty() ? match.captured(4) : match.captured(2);
        
        // Simple tab stop (no multi-cursor support in this implementation)
        QPair<int, QString> stop;
        stop.first = match.capturedStart();
        stop.second = placeholder;
        stops.append(stop);
    }
    
    // Sort by position
    std::sort(stops.begin(), stops.end(),
              [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
                  return a.first < b.first;
              });
}

QString SnippetManager::substituteVariables(const QString &text) const
{
    QString result = text;
    
    // Current date/time variables
    QDateTime now = QDateTime::currentDateTime();
    result.replace("$CURRENT_DATE", now.toString("yyyy-MM-dd"));
    result.replace("$CURRENT_TIME", now.toString("HH:mm:ss"));
    result.replace("$CURRENT_YEAR", now.toString("yyyy"));
    result.replace("$CURRENT_MONTH", now.toString("MM"));
    result.replace("$CURRENT_DAY", now.toString("dd"));
    
    // File variables (would need context from editor)
    // result.replace("$FILENAME", currentFileName);
    // result.replace("$BASENAME", baseName);
    
    // Selection placeholder (would need editor context)
    // result.replace("$TM_SELECTED_TEXT", selectedText);
    
    return result;
}

int SnippetManager::findTabStop(const QString &body, int startPos) const
{
    QRegularExpression re("\\$\\d+|\\$\\{\\d+[^}]*\\}");
    QRegularExpressionMatch match = re.match(body, startPos);
    
    if (match.hasMatch()) {
        return match.capturedStart();
    }
    
    return -1;
}
