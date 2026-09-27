#ifndef STATUSBARWIDGET_H
#define STATUSBARWIDGET_H

#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>

class ThemeManager;
class CodeEditor;

/**
 * Custom status bar with clickable indicators for:
 * - Language name (click to switch)
 * - Encoding (click to change)
 * - Line endings (click to switch)
 * - Indentation (click to configure)
 * - Cursor position (line:column)
 * - Git branch
 * - Error/warning counts
 * - File line count
 */
class StatusBarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit StatusBarWidget(QWidget *parent = nullptr);
    void setThemeManager(ThemeManager *tm);
    // Reverse the indicator order so the bar mirrors with the rest of the
    // window in the mirrored layout.
    void setMirrored(bool mirrored);

    void setLanguage(const QString &language);
    void setEncoding(const QString &encoding);
    void setLineEnding(const QString &ending);
    void setIndentation(const QString &indent);
    void setCursorPosition(int line, int column);
    void setGitBranch(const QString &branch);
    void setErrorCount(int errors, int warnings);
    void setLineCount(int lines);
    void setFileName(const QString &name);
    void setModified(bool modified);

signals:
    void languageClicked();
    void encodingClicked();
    void lineEndingClicked();
    void indentationClicked();
    void gitBranchClicked();
    void errorCountClicked();

private:
    ThemeManager *m_themeManager = nullptr;
    QHBoxLayout *m_layout = nullptr;
    // Kept so the separator frames can be re-ordered along with the labels.
    QFrame *m_sep1 = nullptr;
    QFrame *m_sep2 = nullptr;
    bool m_mirrored = false;
    QLabel *m_fileLabel;
    QLabel *m_languageLabel;
    QLabel *m_encodingLabel;
    QLabel *m_lineEndingLabel;
    QLabel *m_indentLabel;
    QLabel *m_positionLabel;
    QLabel *m_gitBranchLabel;
    QLabel *m_errorLabel;
    QLabel *m_lineCountLabel;

    void applyStyle();
};

#endif // STATUSBARWIDGET_H
