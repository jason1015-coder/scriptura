#ifndef BREADCRUMBBAR_H
#define BREADCRUMBBAR_H

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QList>

class CodeEditor;
class CssBreadcrumbParser;

/**
 * A breadcrumb bar widget that shows:
 * - File path hierarchy (folder > folder > filename)
 * - DOM hierarchy for HTML/CSS files (html > body > div.container > ul > li)
 *
 * Clicking a breadcrumb segment navigates to that level.
 * Placed below the tab bar in the editor container.
 */
class BreadcrumbBarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BreadcrumbBarWidget(QWidget *parent = nullptr);

    // Update the breadcrumb for a given editor
    void updateForEditor(CodeEditor *editor, CssBreadcrumbParser *parser = nullptr);

    // Clear the breadcrumb
    void clear();

signals:
    void segmentClicked(int index, const QString &text);

private:
    void setupUI();
    void clearSegments();
    void addSegment(const QString &text, const QString &tooltip = QString());

    QHBoxLayout *m_segmentsLayout;
    QLabel *m_fileLabel;
    QScrollArea *m_scrollArea;
};

#endif // BREADCRUMBBAR_H
