#include "codeactionui.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QAction>
#include <QShortcut>
#include <QDebug>

#include "lspclient.h"
#include "codeeditor.h"
#include "mainwindow.h"

CodeActionBar::CodeActionBar(QWidget *parent)
    : QFrame(parent)
    , m_layout(new QHBoxLayout(this))
    , m_countLabel(new QLabel(this))
{
    setObjectName("codeActionBar");
    setFrameShape(QFrame::StyledPanel);
    setStyleSheet("QFrame#codeActionBar { background-color: palette(midlight); border-top: 1px solid palette(shadow); padding: 2px; }");
    m_layout->setContentsMargins(8, 2, 8, 2);
    m_layout->setSpacing(8);
    m_countLabel->setStyleSheet("color: palette(text); font-weight: bold; font-size: 11px;");
    m_layout->addWidget(m_countLabel);
    m_layout->addStretch();
    hide();
}

CodeActionBar::~CodeActionBar()
{
}

void CodeActionBar::setActions(const QList<CodeActionItem> &actions)
{
    qDeleteAll(m_buttons);
    m_buttons.clear();
    m_actions = actions;

    if (actions.isEmpty()) {
        m_countLabel->clear();
        hide();
        return;
    }

    int preferredCount = 0;
    for (const auto &a : actions)
        if (a.isPreferred) preferredCount++;

    m_countLabel->setText(QString("%1 fix%2:").arg(actions.size()).arg(actions.size() == 1 ? "" : "es"));

    for (int i = 0; i < actions.size(); ++i) {
        QPushButton *btn = new QPushButton(actions[i].title, this);
        if (actions[i].isPreferred) {
            btn->setStyleSheet("QPushButton { background-color: palette(highlight); color: palette(highlightedText); border: none; padding: 4px 10px; border-radius: 8px; }");
        } else {
            btn->setStyleSheet("QPushButton { border: 1px solid palette(mid); padding: 4px 10px; border-radius: 8px; background: palette(base); }");
            btn->setFlat(true);
        }
        connect(btn, &QPushButton::clicked, this, [this, i]() { onActionClicked(i); });
        m_layout->insertWidget(m_layout->count() - 1, btn);
        m_buttons.append(btn);
    }
    show();
}

void CodeActionBar::clearActions()
{
    qDeleteAll(m_buttons);
    m_buttons.clear();
    m_actions.clear();
    hide();
}

void CodeActionBar::onActionClicked(int index)
{
    if (index < 0 || index >= m_actions.size())
        return;
    emit actionSelected(m_actions[index].title, m_actions[index].kind, index);
}

CodeActionController::CodeActionController(QObject *parent)
    : QObject(parent)
    , m_editor(nullptr)
    , m_client(nullptr)
    , m_bar(nullptr)
    , m_parent(nullptr)
    , m_pendingRequestId(0)
    , m_havePendingRequest(false)
{
}

CodeActionController::~CodeActionController()
{
    if (m_bar) {
        m_bar->hide();
        delete m_bar;
        m_bar = nullptr;
    }
}

void CodeActionController::attach(CodeEditor *editor, LspClient *client, const QString &fileUri)
{
    m_currentUri = fileUri;
    attach(editor, client);
}

void CodeActionController::attach(CodeEditor *editor, LspClient *client)
{
    m_editor = editor;
    m_client = client;
    m_parent = editor ? editor->parentWidget() : nullptr;

    if (m_bar) {
        m_bar->hide();
        delete m_bar;
        m_bar = nullptr;
    }

    if (!editor || !client)
        return;

    m_bar = new CodeActionBar(editor);
    m_parent = editor->parentWidget();
    connect(m_bar, &CodeActionBar::actionSelected, this, &CodeActionController::onActionSelected);

    QShortcut *shortcut = new QShortcut(QKeySequence("Ctrl+."), editor);
    connect(shortcut, &QShortcut::activated, this, &CodeActionController::onShortcutActivated);
}

void CodeActionController::showCurrent()
{
    if (!m_bar || !m_client || m_currentDiagnostics.isEmpty())
        return;
    QString uri = currentFileUri();
    if (uri.isEmpty())
        return;
    requestCodeActions(uri);
}

void CodeActionController::hide()
{
    if (m_bar)
        m_bar->hide();
}

void CodeActionController::onDiagnosticsReceived(const QString &uri, const QList<struct LspClient::Diagnostic> &diagnostics)
{
    if (uri != currentFileUri() || diagnostics.isEmpty()) {
        if (uri == currentFileUri()) {
            m_currentDiagnostics.clear();
            if (m_bar)
                m_bar->hide();
        }
        return;
    }

    m_currentDiagnostics = diagnostics;
    if (diagnostics.size() <= 5)
        showCurrent();
}

void CodeActionController::onCodeActionReceived(const QJsonArray &items, int requestId)
{
    if (requestId != m_pendingRequestId)
        return;
    m_havePendingRequest = false;

    QList<CodeActionItem> actions;
    for (const QJsonValue &v : items) {
        if (!v.isObject())
            continue;
        QJsonObject obj = v.toObject();
        CodeActionItem item;
        item.title = obj["title"].toString();
        item.kind = obj["kind"].toString();
        item.isPreferred = obj["isPreferred"].toBool();
        if (!item.title.isEmpty())
            actions.append(item);
    }

    if (m_bar)
        m_bar->setActions(actions);
}

void CodeActionController::onEditorCursorChanged()
{
    Q_UNUSED(this)
    QList<struct LspClient::Diagnostic> filtered;
    if (m_editor && !m_currentDiagnostics.isEmpty()) {
        if (CodeEditor *ce = qobject_cast<CodeEditor*>(m_editor)) {
            int pos = ce->textCursor().position();
            for (const auto &d : m_currentDiagnostics) {
                if (!d.selections.isEmpty() && pos >= d.selections.first().cursor.selectionStart() && pos <= d.selections.first().cursor.selectionEnd()) {
                    filtered.append(d);
                }
            }
            if (!filtered.isEmpty() && (!m_bar || !m_bar->isVisible()))
                showCurrent();
            else if (filtered.isEmpty() && m_bar && m_bar->isVisible())
                m_bar->hide();
        }
    }
}

void CodeActionController::onActionSelected(const QString &actionTitle, const QString &kind, int index)
{
    Q_UNUSED(actionTitle);
    Q_UNUSED(kind);
    Q_UNUSED(index);
    if (m_bar)
        m_bar->hide();
    emit actionTriggered(actionTitle, kind, index);
}

void CodeActionController::onShortcutActivated()
{
    if (m_bar && m_bar->isVisible()) {
        m_bar->hide();
    } else {
        showCurrent();
    }
}

void CodeActionController::requestCodeActions(const QString &uri)
{
    if (!m_client || uri.isEmpty() || m_havePendingRequest || m_currentDiagnostics.isEmpty())
        return;

    int line = 0;
    int col = 0;
    if (CodeEditor *ce = qobject_cast<CodeEditor*>(m_editor)) {
        line = ce->textCursor().blockNumber() + 1;
        col = ce->textCursor().columnNumber() + 1;
    }

    LspClient::Range range;
    range.start.line = line - 1;
    range.start.character = 0;
    range.end.line = line - 1;
    range.end.character = 10000;

    m_pendingRequestId++;
    m_havePendingRequest = true;
    m_client->codeAction(uri, range);
}

QString CodeActionController::currentFileUri() const
{
    if (!m_currentUri.isEmpty())
        return m_currentUri;
    if (!m_editor)
        return {};
    if (QTabWidget *tabs = qobject_cast<QTabWidget*>(m_parent)) {
        int idx = tabs->indexOf(m_editor);
        if (idx >= 0)
            return QUrl::fromLocalFile(tabs->tabToolTip(idx)).toString();
    }
    return {};
}
