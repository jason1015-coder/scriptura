#ifndef THEMEICONS_H
#define THEMEICONS_H

#include <QObject>
#include <QIcon>
#include <QPointer>
#include <QPalette>
#include <QString>
#include <QFileIconProvider>

class QLabel;
class ThemeManager;
class QAbstractButton;
class QAction;

/**
 * @class ThemeIcons
 * @brief 主題感知的圖標管理員
 *
 * 許多 UI 圖標是單色 SVG (使用 currentColor / 固定黑色)，在淺色主題下可見，
 * 但在深色或高對比主題下會與背景融為一體而「消失」。
 *
 * ThemeIcons 在載入時將 SVG 重新著色 (tint) 為目前調色盤的前景色，
 * 並在每次主題切換時，自動重新著色所有已註冊的圖標，
 * 確保圖標在 light / dark / high-contrast 下都保持足夠對比與可見性。
 */
class ThemeIcons : public QObject
{
    Q_OBJECT
public:
    enum class Role {
        Normal,    ///< 一般狀態：QPalette::WindowText
        Selected,  ///< 選取/高亮背景上的文字：QPalette::HighlightedText
        Disabled,  ///< 停用狀態：QPalette::Disabled | QPalette::WindowText
        Accent,    ///< 強調色：QPalette::Highlight
        Svg        ///< 主題專用的 SVG 圖標色 (從 ThemeManager::svgColor 取得)
    };
    Q_ENUM(Role)

    /**
     * @brief 取得全域單例
     */
    static ThemeIcons* instance();

    /**
     * @brief 取回「目前主題」下重新著色過的圖標 (一次性使用)
     * @param path 資源路徑，例如 ":/icons/close.svg"
     * @param role 著色角色，決定使用哪一種調色盤顏色
     */
    QIcon icon(const QString& path, Role role = Role::Normal) const;

    /**
     * @brief 取回「原始」(不重新著色) 圖標，適用於品牌/多彩圖標
     */
    QIcon rawIcon(const QString& path) const;

    /**
     * @brief 設定並追蹤按鈕圖標，主題切換時自動重新著色
     */
    void setIcon(QAbstractButton* button, const QString& path, Role role = Role::Normal);

    /**
     * @brief 設定並追蹤動作圖標，主題切換時自動重新著色
     */
    void setIcon(QAction* action, const QString& path, Role role = Role::Normal);

    /**
     * @brief 設定並追蹤標籤上的圖標 (以 pixmap 呈現)，主題切換時自動重新著色
     */
    void setIcon(QLabel* label, const QString& path, Role role = Role::Normal,
                 const QSize& size = QSize());

    /**
     * @brief 設定 ThemeManager 實例以取得主題專用 SVG 色
     */
    void setThemeManager(const ThemeManager *mgr);

public slots:
    /**
     * @brief 用目前主題顏色重新著色所有已追蹤的目標
     */
    void recolorAll();

private:
    explicit ThemeIcons(QObject* parent = nullptr);

    QColor colorForRole(Role role) const;
    QIcon makeIcon(const QString& path, const QColor& color) const;

    const ThemeManager *m_themeManager = nullptr;

    struct Entry {
        QPointer<QObject> target;  ///< 弱引用，目標銷毀後自動失效
        QString path;
        Role role;
        QSize size;                ///< 僅 QLabel 使用
    };

    QList<Entry> m_entries;
};

/**
 * @class ThemeFileIconProvider
 * @brief 讓 QFileSystemModel / QFileDialog 使用主題感知的檔案/資料夾圖標
 *
 * 預設的 QFileIconProvider 回傳系統圖標，在應用程式的淺色主題下常因對比不足
 * 而難以辨識。此提供者改為回傳 ThemeIcons 重新著色過的 SVG，
 * 確保在 light / dark / high-contrast 下都能清楚可見。
 */
class ThemeFileIconProvider : public QFileIconProvider
{
public:
    using QFileIconProvider::QFileIconProvider;

    QIcon icon(IconType type) const override;
    QIcon icon(const QFileInfo& info) const override;
};

#endif // THEMEICONS_H
