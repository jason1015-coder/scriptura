#include "shortcuteditor.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QSettings>
#include <QKeyEvent>

void ShortcutEditorWidget::keyPressEvent(QKeyEvent *event)
{
    if (!m_recording) {
        QWidget::keyPressEvent(event);
        return;
    }

    int key = event->key();
    Qt::KeyboardModifiers mods = event->modifiers();

    if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta) {
        return;
    }

    QKeySequence seq(key | mods);
    if (seq.isEmpty()) return;

    if (hasConflict(m_recordingAction, seq)) {
        QMessageBox::warning(this, tr("Shortcut Conflict"),
            tr("This shortcut is already assigned to another action."));
    } else {
        m_shortcuts[m_recordingAction].currentShortcut = seq;
        populateTree();
        emit shortcutChanged(m_recordingAction, seq);
    }

    m_recording = false;
    m_recordBtn->setText(tr("Record"));
    m_recordBtn->setEnabled(true);
    event->accept();
}

ShortcutEditorWidget::ShortcutEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Search bar
    QHBoxLayout *searchLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search shortcuts..."));
    m_recordBtn = new QPushButton(tr("Record"), this);
    m_resetBtn = new QPushButton(tr("Reset All"), this);
    searchLayout->addWidget(m_searchEdit, 1);
    searchLayout->addWidget(m_recordBtn);
    searchLayout->addWidget(m_resetBtn);
    layout->addLayout(searchLayout);

    // Tree widget
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({tr("Action"), tr("Shortcut")});
    m_tree->setColumnWidth(0, 300);
    m_tree->setRootIsDecorated(true);
    m_tree->setAlternatingRowColors(true);
    layout->addWidget(m_tree, 1);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &ShortcutEditorWidget::onSearchChanged);
    connect(m_recordBtn, &QPushButton::clicked, this, &ShortcutEditorWidget::onRecordClicked);
    connect(m_resetBtn, &QPushButton::clicked, this, &ShortcutEditorWidget::onResetClicked);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &ShortcutEditorWidget::onItemDoubleClicked);

    loadShortcuts();
}

void ShortcutEditorWidget::loadShortcuts()
{
    m_shortcuts.clear();
    QSettings settings;
    Q_UNUSED(settings);

    // Default shortcuts
    struct DefaultShortcut { QString action; QString category; QString shortcut; };
    DefaultShortcut defaults[] = {
        {"Save", "File", "Ctrl+S"},
        {"Save As", "File", "Ctrl+Shift+S"},
        {"Open File", "File", "Ctrl+O"},
        {"Open Project", "File", "Ctrl+Shift+O"},
        {"Close Tab", "File", "Ctrl+W"},
        {"New File", "File", "Ctrl+N"},
        {"Undo", "Edit", "Ctrl+Z"},
        {"Redo", "Edit", "Ctrl+Y"},
        {"Cut", "Edit", "Ctrl+X"},
        {"Copy", "Edit", "Ctrl+C"},
        {"Paste", "Edit", "Ctrl+V"},
        {"Find", "Edit", "Ctrl+F"},
        {"Replace", "Edit", "Ctrl+H"},
        {"Find Next", "Edit", "Ctrl+G"},
        {"Find Previous", "Edit", "Ctrl+Shift+G"},
        {"Select All", "Edit", "Ctrl+A"},
        {"Go to Definition", "Navigation", "F12"},
        {"Go to Declaration", "Navigation", "Ctrl+F12"},
        {"Find in Project", "Navigation", "Ctrl+Shift+F"},
        {"Command Palette", "Navigation", "Ctrl+Shift+P"},
        {"Universal Search", "Navigation", "Ctrl+P"},
        {"Rename Symbol", "Refactoring", "F2"},
        {"Toggle Bookmark", "Bookmarks", "Ctrl+B"},
        {"Next Bookmark", "Bookmarks", "Shift+F2"},
        {"Previous Bookmark", "Bookmarks", "Ctrl+Shift+F2"},
        {"Toggle Fold", "View", "Ctrl+Shift+["},
        {"Toggle Sidebar", "View", "Ctrl+B"},
        {"Zen Mode", "View", "Ctrl+Shift+Alt+Z"},
        {"Toggle Terminal", "View", "Ctrl+`"},
        {"Run / Debug", "Debug", "F5"},
        {"Stop Debug", "Debug", "Shift+F5"},
        {"Step Over", "Debug", "F10"},
        {"Step Into", "Debug", "F11"},
        {"Step Out", "Debug", "Shift+F11"},
        {"Continue", "Debug", "Ctrl+F5"},
        {"Toggle Breakpoint", "Debug", "F9"},
        {"Format Document", "Tools", "Ctrl+Shift+I"},
        {"Toggle Comment", "Tools", "Ctrl+/"},
        {"Toggle Word Wrap", "View", "Alt+Z"},
    };

    for (const auto &def : defaults) {
        QString saved = settings.value("shortcuts/" + def.action).toString();
        QKeySequence seq = saved.isEmpty() ? QKeySequence(def.shortcut) : QKeySequence(saved);
        m_shortcuts[def.action] = {def.action, def.category, QKeySequence(def.shortcut), seq};
    }

    populateTree();
}

void ShortcutEditorWidget::populateTree()
{
    m_tree->clear();
    QMap<QString, QTreeWidgetItem*> categories;

    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it) {
        const ShortcutEntry &entry = it.value();

        if (!categories.contains(entry.category)) {
            QTreeWidgetItem *catItem = new QTreeWidgetItem(m_tree);
            catItem->setText(0, entry.category);
            catItem->setExpanded(true);
            QFont font = catItem->font(0);
            font.setBold(true);
            catItem->setFont(0, font);
            categories[entry.category] = catItem;
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(categories[entry.category]);
        item->setText(0, entry.actionName);
        item->setText(1, entry.currentShortcut.toString());
        item->setData(0, Qt::UserRole, entry.actionName);
        if (entry.currentShortcut != entry.defaultShortcut) {
            item->setForeground(1, QColor(100, 200, 100)); // Green for customized
        }
    }
}

void ShortcutEditorWidget::onRecordClicked()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item || !item->parent()) return;

    QString actName = item->data(0, Qt::UserRole).toString();
    if (actName.isEmpty()) return;

    m_recording = true;
    m_recordingAction = actName;
    m_recordBtn->setText(tr("Press keys..."));
    m_recordBtn->setEnabled(false);
    setFocus();
}

void ShortcutEditorWidget::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (!item || !item->parent() || column != 1) return;
    m_tree->setCurrentItem(item);
    onRecordClicked();
}

void ShortcutEditorWidget::onResetClicked()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Reset Shortcuts"), tr("Reset all shortcuts to defaults?"),
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) resetToDefaults();
}

void ShortcutEditorWidget::resetToDefaults()
{
    for (auto it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it) {
        it->currentShortcut = it->defaultShortcut;
    }
    populateTree();
    QSettings().remove("shortcuts");
}

void ShortcutEditorWidget::saveShortcuts()
{
    QSettings settings;
    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it) {
        if (it->currentShortcut != it->defaultShortcut) {
            settings.setValue("shortcuts/" + it->actionName, it->currentShortcut.toString());
        } else {
            settings.remove("shortcuts/" + it->actionName);
        }
    }
}

void ShortcutEditorWidget::onSearchChanged(const QString &text)
{
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *cat = m_tree->topLevelItem(i);
        bool anyVisible = false;
        for (int j = 0; j < cat->childCount(); ++j) {
            QTreeWidgetItem *child = cat->child(j);
            bool match = text.isEmpty() ||
                         child->text(0).contains(text, Qt::CaseInsensitive) ||
                         child->text(1).contains(text, Qt::CaseInsensitive);
            child->setHidden(!match);
            if (match) anyVisible = true;
        }
        cat->setHidden(!anyVisible);
    }
}

bool ShortcutEditorWidget::hasConflict(const QString &action, const QKeySequence &shortcut) const
{
    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it) {
        if (it->actionName != action && it->currentShortcut == shortcut) return true;
    }
    return false;
}
