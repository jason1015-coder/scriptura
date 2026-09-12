#ifndef BREADCRUMB_H
#define BREADCRUMB_H

#include <QWidget>
#include <QString>
#include <QPlainTextEdit>

class Breadcrumb : public QWidget
{
    Q_OBJECT
public:
    explicit Breadcrumb(QPlainTextEdit *editor, QWidget *parent = nullptr);

    void setFilePath(const QString &path);
    void setSymbolPath(const QString &path);
    void updateFromCursor();

    QString filePath() const { return m_filePath; }
    QString symbolPath() const { return m_symbolPath; }
    QStringList filePathParts() const { return parseFilePath(m_filePath); }

signals:
    void breadcrumbClicked(const QString &path);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QPlainTextEdit *m_editor;
    QString m_filePath;
    QString m_symbolPath;
    
    QStringList parseFilePath(const QString &path) const;
    int hitTest(const QPoint &pos) const;
};

#endif // BREADCRUMB_H
