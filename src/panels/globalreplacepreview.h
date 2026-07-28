#ifndef GLOBALREPLACEPREVIEW_H
#define GLOBALREPLACEPREVIEW_H

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>

/**
 * Global Replace Preview panel showing replacements across project.
 * Features:
 * - Preview all replacements before applying
 * - Selective replacement (checkbox per match)
 * - Diff view for each replacement
 * - Undo support
 */
class GlobalReplacePreview : public QWidget
{
    Q_OBJECT
public:
    explicit GlobalReplacePreview(QWidget *parent = nullptr);

    // Core operations
    void setSearchPattern(const QString &pattern);
    void setReplaceText(const QString &replacement);
    void searchInProject(const QString &projectPath);
    void applyReplacements();
    void cancelReplacements();
    
    // Configuration
    void setCaseSensitive(bool enabled);
    void setUseRegex(bool enabled);
    void setWholeWord(bool enabled);

signals:
    void replacementApplied(int count);
    void replacementCancelled();
    void matchSelected(const QString &filePath, int line, int column);

private slots:
    void onSearchClicked();
    void onApplyClicked();
    void onCancelClicked();
    void onSelectAllClicked();
    void onDeselectAllClicked();
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);

private:
    void setupUI();
    void performSearch();
    void updatePreview();
    void clearResults();
    
    QLineEdit *m_searchEdit;
    QLineEdit *m_replaceEdit;
    QCheckBox *m_caseSensitive;
    QCheckBox *m_useRegex;
    QCheckBox *m_wholeWord;
    QTreeWidget *m_resultsTree;
    QLabel *m_statusLabel;
    QPushButton *m_searchButton;
    QPushButton *m_applyButton;
    QPushButton *m_cancelButton;
    QPushButton *m_selectAllButton;
    QPushButton *m_deselectAllButton;
    
    QString m_projectPath;
    int m_totalMatches;
    int m_selectedMatches;
};

#endif // GLOBALREPLACEPREVIEW_H
