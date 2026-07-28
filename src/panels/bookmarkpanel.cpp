#include "bookmarkpanel.h"
#include <QLabel>
#include <QFileInfo>

BookmarkPanelWidget::BookmarkPanelWidget(BookmarkManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    QLabel *title = new QLabel(tr("Bookmarks"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({tr("File"), tr("Line"), tr("Text")});
    m_tree->setColumnWidth(0, 200);
    m_tree->setColumnWidth(1, 50);
    m_tree->setAlternatingRowColors(true);
    layout->addWidget(m_tree, 1);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_jumpBtn = new QPushButton(tr("Jump"), this);
    m_removeBtn = new QPushButton(tr("Remove"), this);
    m_clearBtn = new QPushButton(tr("Clear All"), this);
    btnLayout->addWidget(m_jumpBtn);
    btnLayout->addWidget(m_removeBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_clearBtn);
    layout->addLayout(btnLayout);

    connect(m_jumpBtn, &QPushButton::clicked, this, &BookmarkPanelWidget::onJumpClicked);
    connect(m_removeBtn, &QPushButton::clicked, this, &BookmarkPanelWidget::onRemoveClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &BookmarkPanelWidget::onClearAllClicked);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &BookmarkPanelWidget::onItemDoubleClicked);

    if (m_manager) {
        connect(m_manager, &BookmarkManager::bookmarksChanged, this, &BookmarkPanelWidget::onBookmarksChanged);
    }

    populateTree();
}

void BookmarkPanelWidget::populateTree()
{
    m_tree->clear();
    if (!m_manager) return;

    QList<Bookmark> bookmarks = m_manager->bookmarks();
    QMap<QString, QTreeWidgetItem*> fileGroups;

    for (const Bookmark &bm : bookmarks) {
        QString file = bm.filePath.isEmpty() ? tr("(untitled)") : QFileInfo(bm.filePath).fileName();

        if (!fileGroups.contains(bm.filePath)) {
            QTreeWidgetItem *group = new QTreeWidgetItem(m_tree);
            group->setText(0, file);
            QFont font = group->font(0);
            font.setBold(true);
            group->setFont(0, font);
            fileGroups[bm.filePath] = group;
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(fileGroups[bm.filePath]);
        item->setText(0, bm.filePath);
        item->setText(1, QString::number(bm.line + 1));
        item->setText(2, bm.text);
        item->setData(0, Qt::UserRole, bm.id);
    }

    m_tree->expandAll();
}

void BookmarkPanelWidget::onJumpClicked()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item || !item->parent()) return;

    int id = item->data(0, Qt::UserRole).toInt();
    if (m_manager) {
        m_manager->goToBookmark(id);
        emit bookmarkActivated(item->text(0), item->text(1).toInt() - 1);
    }
}

void BookmarkPanelWidget::onRemoveClicked()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item || !item->parent()) return;

    int id = item->data(0, Qt::UserRole).toInt();
    if (m_manager) {
        m_manager->removeBookmark(id);
        emit bookmarkRemoved(id);
        populateTree();
    }
}

void BookmarkPanelWidget::onClearAllClicked()
{
    if (m_manager) {
        m_manager->removeAllBookmarks();
        populateTree();
    }
}

void BookmarkPanelWidget::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    m_tree->setCurrentItem(item);
    onJumpClicked();
}

void BookmarkPanelWidget::onBookmarksChanged()
{
    populateTree();
}

void BookmarkPanelWidget::refresh()
{
    populateTree();
}
