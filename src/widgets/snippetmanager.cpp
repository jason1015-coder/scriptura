#include "snippetmanager.h"
#include "rust_adapter.h"
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QSettings>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QRegularExpression>
#include <QApplication>

SnippetManager::SnippetManager(QObject *parent)
    : QObject(parent)
    , m_currentTabStopIndex(-1)
    , m_store(new RustSnippetStoreAdapter(this))
{
    m_store->load(QSettings().value("snippets").toByteArray());
}

void SnippetManager::addSnippet(const Snippet &snippet)
{
    if (!m_store->add(snippet)) return;
    emit snippetAdded(snippet.id);
    emit snippetsChanged();
}

void SnippetManager::updateSnippet(const Snippet &snippet)
{
    if (!m_store->update(snippet)) return;
    emit snippetsChanged();
}

void SnippetManager::removeSnippet(const QString &id)
{
    if (m_store->remove(id)) {
        emit snippetRemoved(id);
        emit snippetsChanged();
    }
}

Snippet SnippetManager::snippetById(const QString &id) const
{
    return m_store->get(id);
}

QList<Snippet> SnippetManager::snippetsForLanguage(const QString &language) const
{
    return m_store->forLanguage(language);
}

QList<Snippet> SnippetManager::allSnippets() const
{
    return m_store->all();
}

QStringList SnippetManager::snippetPrefixes() const
{
    return m_store->prefixes();
}

void SnippetManager::insertSnippet(QPlainTextEdit *editor, const Snippet &snippet)
{
    if (!editor) return;

    // Substitute datetime variables (runtime values from Qt).
    QVariantMap vars;
    vars["CURRENT_DATE"] = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    vars["CURRENT_TIME"] = QDateTime::currentDateTime().toString("HH:mm:ss");
    vars["CURRENT_YEAR"] = QDateTime::currentDateTime().toString("yyyy");
    vars["CURRENT_MONTH"] = QDateTime::currentDateTime().toString("MM");
    vars["CURRENT_DAY"] = QDateTime::currentDateTime().toString("dd");
    QString body = RustSnippetStoreAdapter::substituteVariables(snippet.body, vars);

    clearTabStops();

    const QString json = RustTextBufferAdapter::expandSnippet(body, QFileInfo(editor->objectName()).fileName());
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    QString expandedText = body;
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        expandedText = doc.object().value("text").toString(expandedText);
        QJsonArray stops = doc.object().value("tabStops").toArray();
        for (const QJsonValue &v : stops) {
            const QJsonObject o = v.toObject();
            int byteOff = o.value("offset").toInt(-1);
            int byteLen = o.value("len").toInt(0);
            if (byteOff < 0) continue;
            int utf16Pos = QString::fromUtf8(expandedText.toUtf8().left(byteOff)).size();
            int utf16Len = QString::fromUtf8(expandedText.toUtf8().mid(byteOff, byteLen)).size();
            TabStop ts;
            ts.position = utf16Pos;
            ts.length = utf16Len;
            ts.placeholder = expandedText.mid(utf16Pos, utf16Len);
            m_tabStops.append(ts);
        }
    } else {
        QList<QPair<int, QString>> parsedStops;
        parseTabStops(body, parsedStops);
        for (const auto &ps : parsedStops) {
            TabStop ts;
            ts.position = ps.first;
            ts.length = ps.second.isEmpty() ? 0 : ps.second.length();
            ts.placeholder = ps.second;
            m_tabStops.append(ts);
        }
    }

    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();
    cursor.insertText(expandedText);
    int basePos = cursor.position() - expandedText.length();
    for (int i = 0; i < m_tabStops.size(); ++i) {
        m_tabStops[i].position += basePos;
    }
    cursor.endEditBlock();
    editor->setTextCursor(cursor);

    if (!m_tabStops.isEmpty()) {
        m_currentTabStopIndex = 0;
        nextTabStop(editor);
    }

    emit snippetInserted(snippet.id);
}

bool SnippetManager::hasSnippetForPrefix(const QString &prefix, const QString &language) const
{
    return m_store->hasPrefix(prefix, language);
}

Snippet SnippetManager::snippetForPrefix(const QString &prefix, const QString &language) const
{
    return m_store->findForPrefix(prefix, language);
}

void SnippetManager::nextTabStop(QPlainTextEdit *editor)
{
    if (!editor || m_tabStops.isEmpty()) return;
    m_currentTabStopIndex++;
    if (m_currentTabStopIndex >= m_tabStops.size()) {
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
    settings.setValue("snippets", m_store->save());
}

void SnippetManager::loadFromSettings()
{
    QSettings settings;
    const QByteArray data = settings.value("snippets").toByteArray();
    if (data.isEmpty()) {
        m_store->load("[]");
        return;
    }
    m_store->load(QString::fromUtf8(data));
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
    size_t added = m_store->importSnippets(QString::fromUtf8(data));
    if (added > 0) {
        emit snippetsChanged();
    }
    return true;
}

bool SnippetManager::exportSnippets(const QString &filePath) const
{
    QJsonArray arr;
    for (const Snippet &s : m_store->all()) {
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

int SnippetManager::snippetCount() const
{
    return m_store->all().size();
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

        QPair<int, QString> stop;
        stop.first = match.capturedStart();
        stop.second = placeholder;
        stops.append(stop);
    }

    std::sort(stops.begin(), stops.end(),
              [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
                  return a.first < b.first;
              });
}
