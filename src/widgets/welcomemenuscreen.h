#ifndef WELCOMEMENUSCREEN_H
#define WELCOMEMENUSCREEN_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QStringList>
#include <QSettings>
#include <QFileInfo>
#include <QScreen>
#include <QApplication>
#include <QFrame>
#include <QScrollArea>
#include <QMouseEvent>

class WelcomeMenuScreen : public QWidget
{
    Q_OBJECT
public:
    explicit WelcomeMenuScreen(QWidget *parent = nullptr);

    void setThemeBackground(const QColor &color);
    void loadRecentProjects();

    // Sentinel helpers (static — no instance needed)
    static QString storagePath();
    static bool isFirstLaunch();
    static void markLaunched();

signals:
    void openProjectRequested();
    void cloneRequested(const QString &gitUrl);
    void recentProjectSelected(const QString &path);
    void newFileRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void setupUI();
    void updateRecentProjectsList();

    QListWidget *m_recentProjectsList;
    QStringList m_recentProjects;
    static constexpr int MAX_RECENT_PROJECTS = 10;

    // Window dragging state
    bool m_dragging = false;
    QPoint m_dragStartPos;

    // Window control buttons
    QPushButton *m_minBtn = nullptr;
    QPushButton *m_maxBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;
    bool m_maximized = false;

    // Action buttons (for icon recoloring)
    QPushButton *m_cloneBtn = nullptr;
    QPushButton *m_newFileBtn = nullptr;
};

#endif // WELCOMEMENUSCREEN_H
