#ifndef CODEACTIONUI_H
#define CODEACTIONUI_H

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFrame>
#include <QList>
#include <QString>
#include <QLabel>
#include <QJsonArray>
#include <QPointer>
#include "codeeditor.h"

class RustLspClientAdapter;

struct CodeActionItem {
    QString title;
    QString kind;
    bool isPreferred;
};

class CodeActionBar : public QFrame
{
    Q_OBJECT
public:
    explicit CodeActionBar(QWidget *parent = nullptr);
    ~CodeActionBar() override;

    void setActions(const QList<CodeActionItem> &actions);
    void clearActions();
    bool hasActions() const { return !m_actions.isEmpty(); }

signals:
    void actionSelected(const QString &actionTitle, const QString &kind, int index);

private slots:
    void onActionClicked(int index);

private:
    QHBoxLayout *m_layout;
    QList<CodeActionItem> m_actions;
    QList<QPushButton*> m_buttons;
    QLabel *m_countLabel;
};

class CodeActionController : public QObject
{
    Q_OBJECT
public:
    explicit CodeActionController(QObject *parent = nullptr);
    ~CodeActionController() override;

    void attach(CodeEditor *editor, RustLspClientAdapter *client);
    void attach(CodeEditor *editor, RustLspClientAdapter *client, const QString &fileUri);
    void showCurrent();
    void hide();
    bool isVisible() const { return m_bar && m_bar->isVisible(); }

    void onDiagnosticsReceived(const QString &uri, const QJsonArray &diagnostics);
    void onCodeActionReceived(const QJsonArray &items, int requestId);
    void onActionSelected(const QString &actionTitle, const QString &kind, int index);
    void onEditorCursorChanged();
    void onShortcutActivated();

signals:
    void actionTriggered(const QString &actionTitle, const QString &kind, int index);

private:
    void requestCodeActions(const QString &uri);
    QString currentFileUri() const;
    QJsonArray m_currentDiagnostics;

    CodeEditor *m_editor;
    RustLspClientAdapter *m_client;
    // QPointer so the bar auto-nulls when its owning editor/tab is destroyed;
    // a raw pointer here dangles -> m_bar->hide() in attach()/~Controller
    // dereferences freed memory and segfaults when switching files.
    QPointer<CodeActionBar> m_bar;
    QWidget *m_parent;
    QString m_currentUri;
    int m_pendingRequestId;
    bool m_havePendingRequest;
};

#endif // CODEACTIONUI_H
