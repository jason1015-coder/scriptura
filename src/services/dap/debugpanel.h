#ifndef DEBUGPANEL_H
#define DEBUGPANEL_H

#include <QWidget>
#include <QTreeView>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QSplitter>
#include <QJsonArray>

class RustDapClientAdapter;

class DebugPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DebugPanel(QWidget *parent = nullptr);
    void setClient(RustDapClientAdapter *client);

    void setStack(const QJsonArray &frames);
    void addVariables(const QJsonArray &variables);
    void clearVariables();
    void showEvaluation(const QString &expression, const QString &result);
    void showWatchEvaluation(const QString &expression, const QString &result);
    void addWatch(const QString &expr);
    void clearWatches();

signals:
    void frameActivated(int index);
    void watchRequested(const QString &expr);
    void evaluateRequested(const QString &expr);
    void evaluationContextReceived(const QString &expr, const QString &result, const QString &context);

private slots:
    void onStackActivated(const QModelIndex &index);
    void onWatchReturnPressed();
    void onEvalReturnPressed();
    void onExpand(const QModelIndex &index);

private:
    QTreeView *m_stackTree;
    QTreeView *m_watchesTree;
    QTreeView *m_variablesTree;
    QTabWidget *m_tabs;
    QLineEdit *m_watchEdit;
    QLineEdit *m_evalEdit;
    RustDapClientAdapter *m_client;
};

#endif // DEBUGPANEL_H
