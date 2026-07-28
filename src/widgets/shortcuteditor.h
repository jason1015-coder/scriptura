#ifndef SHORTCUTEDITOR_H
#define SHORTCUTEDITOR_H

#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QMap>
#include <QKeySequence>

struct ShortcutEntry {
    QString actionName;
    QString category;
    QKeySequence defaultShortcut;
    QKeySequence currentShortcut;
};

class ShortcutEditorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ShortcutEditorWidget(QWidget *parent = nullptr);

    void loadShortcuts();
    void saveShortcuts();
    void resetToDefaults();

signals:
    void shortcutChanged(const QString &action, const QKeySequence &shortcut);

private slots:
    void onRecordClicked();
    void onResetClicked();
    void onSearchChanged(const QString &text);
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void populateTree();
    bool hasConflict(const QString &action, const QKeySequence &shortcut) const;

    QTreeWidget *m_tree;
    QLineEdit *m_searchEdit;
    QPushButton *m_recordBtn;
    QPushButton *m_resetBtn;
    QMap<QString, ShortcutEntry> m_shortcuts;
    bool m_recording = false;
    QString m_recordingAction;
};

#endif // SHORTCUTEDITOR_H
