#ifndef REGEXTESTER_H
#define REGEXTESTER_H

#include <QWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QListWidget>
#include <QCheckBox>
#include <QPushButton>

/**
 * Regex Tester panel for testing regular expressions against sample text.
 * Features:
 * - Real-time pattern matching
 * - Match highlighting in sample text
 * - Match list with groups
 * - Regex flags (case insensitive, multiline, etc.)
 * - Save/load patterns
 */
class RegexTester : public QWidget
{
    Q_OBJECT
public:
    explicit RegexTester(QWidget *parent = nullptr);

signals:
    void regexCopied(const QString &pattern);
    void patternSaved(const QString &name, const QString &pattern);

private slots:
    void onPatternChanged();
    void onTextChanged();
    void onFlagChanged();
    void onSavePattern();
    void onLoadPattern();
    void onCopyPattern();

private:
    void setupUI();
    void performMatch();
    void highlightMatches();
    void updateMatchList();
    void updateStatus();
    
    QLineEdit *m_patternEdit;
    QPlainTextEdit *m_textEdit;
    QLabel *m_statusLabel;
    QListWidget *m_matchList;
    QCheckBox *m_caseInsensitive;
    QCheckBox *m_dotMatchesNewline;
    QCheckBox *m_multiline;
    QCheckBox *m_global;
    QPushButton *m_saveButton;
    QPushButton *m_loadButton;
    QPushButton *m_copyButton;
    
    // Current matches
    struct Match {
        int start;
        int length;
        QString text;
        QStringList groups;
    };
    QList<Match> m_matches;
};

#endif // REGEXTESTER_H
