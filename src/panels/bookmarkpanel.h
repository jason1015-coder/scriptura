#ifndef BOOKMARKPANEL_H
#define BOOKMARKPANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include "bookmarkmanager.h"

class BookmarkPanelWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BookmarkPanelWidget(BookmarkManager *manager, QWidget *parent = nullptr);

    void setManager(BookmarkManager *manager);
    void refresh();
    int bookmarkCount() const { return m_manager ? m_manager->bookmarkCount() : 0; }

signals:
    void bookmarkActivated(const QString &filePath, int line);
    void bookmarkRemoved(int id);
    void navigateToBookmark(const QString &filePath, int line);

private slots:
    void onJumpClicked();
    void onRemoveClicked();
    void onClearAllClicked();
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onBookmarksChanged();

private:
    void populateTree();

    QTreeWidget *m_tree;
    QPushButton *m_jumpBtn;
    QPushButton *m_removeBtn;
    QPushButton *m_clearBtn;
    BookmarkManager *m_manager;
};

#endif // BOOKMARKPANEL_H
