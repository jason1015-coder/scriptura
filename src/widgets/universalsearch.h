#ifndef UNIVERSALSEARCH_H
#define UNIVERSALSEARCH_H

#include <QFrame>
#include <QLineEdit>
#include <QListWidget>
#include <QString>
#include <QVector>
#include <QFileSystemModel>
#include <functional>

struct SearchResult {
    enum Category { Command = 0, File = 1, Setting = 2, Theme = 3 };
    Category category;
    QString label;
    QString sublabel;
    std::function<void()> action;

    bool operator<(const SearchResult &other) const {
        if (category != other.category) return category < other.category;
        return label.toLower() < other.label.toLower();
    }
};

class UniversalSearchPopup : public QFrame
{
    Q_OBJECT
public:
    explicit UniversalSearchPopup(QLineEdit *searchField, QWidget *parent = nullptr);

    void registerResult(const SearchResult &result);
    void registerResults(const QVector<SearchResult> &results);
    void clearResults();

    // File model for searching file names (optional)
    void setFileModel(QFileSystemModel *model, const QString &rootPath);

    void showPopup();
    void hidePopup();

    // Open the search popup from a shortcut (e.g. Ctrl+Shift+P) even when the
    // field is empty — lists all registered commands/settings/themes.
    void openSearch();

signals:
    void fileOpenRequested(const QString &filePath);
    void dismissed();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onSearchTextChanged(const QString &text);

private:
    void filterResults(const QString &query);
    void activateItem(QListWidgetItem *item);
    int fuzzyScore(const QString &pattern, const QString &text) const;

    QLineEdit *m_searchField;
    QListWidget *m_listWidget;

    // Permanent registered results (commands, settings, themes)
    QVector<SearchResult> m_permanentResults;
    // Total results including temporary file matches
    QVector<SearchResult> m_allResults;

    QFileSystemModel *m_fileModel = nullptr;
    QString m_rootPath;
};

#endif // UNIVERSALSEARCH_H
