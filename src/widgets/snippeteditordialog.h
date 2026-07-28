#ifndef SNIPPETEDITORDIALOG_H
#define SNIPPETEDITORDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>

/**
 * Dialog for managing code snippets.
 * Features:
 * - Browse snippets by language
 * - Create/edit/delete snippets
 * - Import/export snippets as JSON
 * - VS Code snippet format compatible
 */
class SnippetEditorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SnippetEditorDialog(QWidget *parent = nullptr);

    void loadSnippets(const QJsonObject &snippets);
    QJsonObject snippets() const { return m_snippets; }

signals:
    void snippetsChanged(const QJsonObject &snippets);

private slots:
    void onSnippetSelected(int index);
    void onNewClicked();
    void onDeleteClicked();
    void onSaveClicked();
    void onImportClicked();
    void onExportClicked();
    void onLanguageChanged(const QString &language);

private:
    void setupUI();
    void populateSnippetList();
    void saveCurrentSnippet();

    QTreeWidget *m_snippetTree;
    QLineEdit *m_prefixEdit;
    QLineEdit *m_descriptionEdit;
    QPlainTextEdit *m_bodyEdit;
    QPlainTextEdit *m_insertTextEdit;
    QPushButton *m_newBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_saveBtn;
    QPushButton *m_importBtn;
    QPushButton *m_exportBtn;
    QLabel *m_statusLabel;

    QJsonObject m_snippets;
    QString m_currentLanguage;
    QString m_currentSnippetName;
};

#endif // SNIPPETEDITORDIALOG_H
