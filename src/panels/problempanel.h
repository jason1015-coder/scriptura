#ifndef PROBLEMPANEL_H
#define PROBLEMPANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QTabBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QJsonArray>

class ProblemPanel : public QWidget
{
    Q_OBJECT
public:
    enum Filter { All = 0, Errors = 1, Warnings = 2, Info = 3 };

    explicit ProblemPanel(QWidget *parent = nullptr);
    ~ProblemPanel();

    void setProblems(const QString &fileUri, const QJsonArray &diagnostics);
    void clearProblems(const QString &fileUri);
    void clearAll();
    int problemCount(Filter filter = All) const;

    void setCurrentFile(const QString &fileUri);
    QString currentFile() const { return m_currentFile; }

signals:
    void problemActivated(const QString &fileUri, int line, int column);
    void filterChanged(Filter filter);

public slots:
    void setFilter(Filter filter);
    void onItemActivated(QTreeWidgetItem *item, int column);

private slots:
    void onFilterTabChanged(int index);
    void onCloseClicked();
    void onProblemsChanged(const QString &uri, const QJsonArray &diags);

private:
    struct ProblemItem {
        QString fileUri;
        int line;
        int column;
        int severity; // 1=Error, 2=Warning, 3=Info, 4=Hint
        QString message;
        QString source;
    };

    void rebuildTree();
    void addProblemItem(const ProblemItem &item);
    QTreeWidgetItem *createTreeItem(const ProblemItem &item) const;
    QString severityIcon(int severity) const;
    QString severityText(int severity) const;
    QColor severityColor(int severity) const;

    QTreeWidget *m_treeWidget;
    QTabBar *m_filterTabs;
    QPushButton *m_closeButton;
    QLabel *m_countLabel;
    QVBoxLayout *m_mainLayout;

    QMap<QString, QJsonArray> m_allProblems;
    QList<ProblemItem> m_filteredProblems;
    Filter m_currentFilter;
    QString m_currentFile;
};

#endif // PROBLEMPANEL_H
