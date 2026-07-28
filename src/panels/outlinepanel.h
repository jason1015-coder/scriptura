#ifndef OUTLINEPANEL_H
#define OUTLINEPANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QList>
#include <QMap>

class RustLspClientAdapter;

// Inline minimal types so this header compiles without full LspClient include
namespace LspSymbolTypes {
    struct Position { int line = 0; int character = 0; };
    struct Range { Position start; Position end; };
    struct SymbolInformation {
        QString name;
        QString kind;
        QString uri;
        QString containerName;
        Range range;
    };
}

/**
 * Outline/Symbol Tree panel showing file symbols from LSP.
 * Features:
 * - Hierarchical symbol display
 * - Symbol kind icons
 * - Filter/search symbols
 * - Click to navigate
 * - Auto-update on file changes
 */
class OutlinePanel : public QWidget
{
    Q_OBJECT
public:
    explicit OutlinePanel(QWidget *parent = nullptr);

    // Core operations
    void setSymbols(const QList<LspSymbolTypes::SymbolInformation> &symbols);
    void clear();
    void refresh();
    
    // Configuration
    void setFilePath(const QString &path);
    void setLspClient(RustLspClientAdapter *client);

signals:
    void symbolActivated(const QString &filePath, int line, int column);
    void filterChanged(const QString &filter);

private slots:
    void onFilterChanged(const QString &text);
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onRefreshClicked();

private:
    void setupUI();
    void populateTree(const QList<LspSymbolTypes::SymbolInformation> &symbols);
    QTreeWidgetItem *createItem(const LspSymbolTypes::SymbolInformation &symbol);
    QIcon iconForKind(const QString &kind) const;
    
    QTreeWidget *m_tree;
    QLineEdit *m_filterEdit;
    QLabel *m_statusLabel;
    QPushButton *m_refreshButton;
    
    QString m_filePath;
    RustLspClientAdapter *m_lspClient;
    QList<LspSymbolTypes::SymbolInformation> m_symbols;
};

#endif // OUTLINEPANEL_H
