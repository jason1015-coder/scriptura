#ifndef DIFFVIEWERWIDGET_H
#define DIFFVIEWERWIDGET_H

#include <QWidget>
#include <QSplitter>
#include <QTextDocument>
#include <QList>
#include <QScrollBar>
#include <QTextEdit>

/**
 * Represents a single diff hunk between two files.
 */
struct DiffHunk {
    int leftStart;
    int leftCount;
    int rightStart;
    int rightCount;
    QStringList leftLines;
    QStringList rightLines;
    QStringList changeTypes; // "added", "removed", "changed", "unchanged"
};

/**
 * Side-by-side diff viewer for comparing two files.
 * Features:
 * - Synchronized scrolling
 * - Highlighted additions/deletions
 * - Navigate between changes
 * - Copy text between panels
 */
class DiffViewerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DiffViewerWidget(QWidget *parent = nullptr);

    // Compare two files
    void compareFiles(const QString &fileLeft, const QString &fileRight);

    // Compare two strings directly
    void compareTexts(const QString &left, const QString &right,
                      const QString &leftTitle = QString(),
                      const QString &rightTitle = QString());

    // Navigate to next/previous change
    void nextChange();
    void previousChange();

    // Get number of changes
    int changeCount() const { return m_hunks.size(); }

    // Current change index
    int currentChangeIndex() const { return m_currentHunkIndex; }

signals:
    void changeNavigated(int index, int total);

private slots:
    void onLeftScrollBarChanged(int value);
    void onRightScrollBarChanged(int value);

private:
    struct DiffLine {
        QString text;
        enum Type { Unchanged, Added, Removed, Modified } type;
    };

    void computeDiff(const QString &left, const QString &right);
    void renderDiff();
    void highlightLine(QTextEdit *edit, int line, const QColor &color);
    void syncScroll(QScrollBar *source, QScrollBar *target);

    QSplitter *m_splitter;
    QTextEdit *m_leftEdit;
    QTextEdit *m_rightEdit;
    QList<DiffHunk> m_hunks;
    int m_currentHunkIndex = -1;
    bool m_syncingScroll = false;

    QString m_leftTitle;
    QString m_rightTitle;
};

#endif // DIFFVIEWERWIDGET_H
