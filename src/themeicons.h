#ifndef THEMEICONS_H
#define THEMEICONS_H

#include <QObject>
#include <QIcon>
#include <QPointer>
#include <QPalette>
#include <QString>
#include <QSize>
#include <QColor>
#include <QCache>
#include <QFileIconProvider>
#include <QHash>

#include <memory>

class QLabel;
class ThemeManager;
class QAbstractButton;
class QAction;
class QSvgRenderer;

class ThemeIconsPrivate;

/**
 * @class ThemeIcons
 * @brief 主題感知的圖標管理員
 *
 * 許多 UI 圖標是單色 SVG (使用 currentColor / 固定黑色)，在淺色主題下可見，
 * 但在深色或高對比主題下會與背景融為一體而「消失」。
 *
 * ThemeIcons 將 SVG 的 alpha 形狀遮罩與「著色」分離處理：
 * 形狀只解析一次，著色結果快取起來，確保圖標在 light / dark /
 * high-contrast 下都保持足夠對比與可見性。
 *
 * 效率設計：
 * - 每個 SVG 以 QSvgRenderer 解析一次並常駐 m_renderers。因為只取 alpha
 *   通道，圖形內容與主題無關，故此快取永不失效。
 * - 著色結果 (SourceIn 合成) 以 (path, deviceSize, color) 為鍵快取於
 *   m_tintCache；主題切換 (recolorAll) 時整池失效並重算。
 * - 只光柵化「實際會被使用」的單一尺寸，而非預先產生多組解析度。QIcon 缺少
 *   精確命中時會自行縮放，預先產生 7 種尺寸只是白白多算 7 次。
 * - 圖標不預先產生 Disabled 變體：Qt 的樣式引擎會自行合成，且合成結果由
 *   QIcon 內部快取。
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
     * @brief 未呼叫 setIconSize() 的按鈕實際會使用的圖標邊長。
     * 對應 Fusion 樣式的 PM_ButtonIconSize；專案 QSS 未設定 icon-size，
     * 因此這是所有未指定尺寸按鈕的實際值。
     */
    static constexpr int DefaultSize = 16;

    /**
     * @brief 取得全域單例
     */
    static ThemeIcons* instance();

    /**
     * @brief 取回「目前主題」下重新著色過、且為指定邏輯尺寸的圖標
     * @param path 資源路徑，例如 ":/icons/close.svg"
     * @param role 著色角色，決定使用哪一種調色盤顏色
     * @param size 邏輯像素邊長；會乘上 devicePixelRatio 後光柵化
     */
    QIcon icon(const QString& path, Role role = Role::Normal,
               int size = DefaultSize) const;

    /**
     * @brief 同 icon()，但直接回傳已著色的 pixmap (QLabel / logo 使用)
     */
    QPixmap pixmap(const QString& path, Role role = Role::Normal,
                   int size = DefaultSize) const;

    /**
     * @brief 以「明確指定的顏色」著色，而非由角色推導。
     *
     * 給已經自行算出目標色的呼叫端（例如歡迎畫面）使用，好處是仍然走同一份
     * 快取，而不必各自重寫一套渲染邏輯。
     */
    QIcon icon(const QString& path, const QColor& color,
               int size = DefaultSize) const;
    QPixmap pixmap(const QString& path, const QColor& color,
                   int size = DefaultSize) const;

    /**
     * @brief 建立「隨選取狀態自動換色」的圖標。
     *
     * 回傳的 QIcon 同時帶有 Normal 與 Selected 兩種樣式，Qt 的樣式引擎會依
     * 目前的 :selected 狀態自行挑選。檔案樹的高亮列與分頁列的被選取分頁都是
     * 畫在 highlight 底色上，若只有一般狀態的顏色，深色主題下會變成
     * 淺字淺底而看不見。
     *
     * 兩種樣式都走同一份著色快取，成本只比 icon() 多一次光柵化。
     */
    QIcon statefulIcon(const QString& path, int size = DefaultSize) const;

    /**
     * @brief 設定並追蹤按鈕圖標，主題切換時自動重新著色
     * @param size 邏輯像素邊長，應與按鈕的 iconSize 一致
     */
    void setIcon(QAbstractButton* button, const QString& path,
                 Role role = Role::Normal, int size = DefaultSize);

    /**
     * @brief 設定並追蹤動作圖標，主題切換時自動重新著色
     */
    void setIcon(QAction* action, const QString& path,
                 Role role = Role::Normal, int size = DefaultSize);

    /**
     * @brief 設定並追蹤標籤上的圖標 (以 pixmap 呈現)，主題切換時自動重新著色
     */
    void setIcon(QLabel* label, const QString& path, Role role = Role::Normal,
                 const QSize& size = QSize());

    /**
     * @brief 設定 ThemeManager 實例以取得主題專用 SVG 色
     */
    void setThemeManager(const ThemeManager *mgr);

    /**
     * @brief 取得某個角色在目前主題下的實際顏色。
     *
     * 供需要記憶化衍生圖標的呼叫端 (例如檔案圖標提供者) 以顏色作為
     * 快取有效性判斷依據。
     */
    QColor colorForRole(Role role) const;

    /**
     * @brief 著色快取的鍵：同一組欄位才會共用同一個已光柵化的 pixmap。
     *
     * 公開出來是為了讓「哪些請求被視為同一個圖標」這條規則可以直接被測試，
     * 而不必去數光柵化次數。deviceSize 是 deviceSize = round(size * dpr)，
     * 與 dpr 分開存放，因為兩個不同的比例可能四捨五入成同一個 deviceSize。
     */
    struct TintKey {
        QString path;
        int deviceSize = 0;
        QColor color;
        // devicePixelRatio is part of the key in its own right: two ratios can
        // round to the same device size (1.0x18 and 1.2x18 both give 22px), and
        // serving one ratio's pixmap to the other would stretch the glyph.
        qreal dpr = 1.0;

        bool operator==(const TintKey& o) const
        {
            return deviceSize == o.deviceSize && color == o.color
                && path == o.path && dpr == o.dpr;
        }
    };

    /**
     * @brief 自程序啟動以來實際執行的 SVG 光柵化次數。
     * 供測試驗證快取確實生效 (第二次取得同一圖標應為 0 次)。
     */
    static int rasterizationCount();

    /**
     * @brief 目前被追蹤且目標仍存活的項目數。診斷與測試用。
     */
    int trackedCount() const { return static_cast<int>(m_entries.size()); }

public slots:
    /**
     * @brief 用目前主題顏色重新著色所有已追蹤的目標
     */
    void recolorAll();

private:
    explicit ThemeIcons(QObject* parent = nullptr);

    /**
     * @brief 解析並快取指定路徑的 SVG 渲染器 (僅解析一次)。
     * @return nullptr 表示路徑無法解析。
     */
    std::shared_ptr<QSvgRenderer> rendererFor(const QString& path) const;

    /**
     * @brief 於單一 QImage 上完成光柵化 + 著色，並依 TintKey 快取。
     */
    QPixmap tintPixmap(const QString& path, const QColor& color, int logicalSize) const;

    /**
     * @brief 移除目標已被銷毀的追蹤項目
     */
    void pruneDeadEntries();

    // 快取鍵：以「裝置像素」而非邏輯像素計，使 dpr 變動時自然 miss 而不會
    // 沿用舊的解析度。
    friend size_t qHash(const ThemeIcons::TintKey& key, uint seed);

    struct Entry {
        QPointer<QObject> target;  ///< 弱引用，目標銷毀後自動失效
        QString path;
        Role role = Role::Normal;
        int size = DefaultSize;     ///< 邏輯像素
    };

    const ThemeManager *m_themeManager = nullptr;

    // 以目標物件為鍵：重複註冊會覆寫而非追加，天然避免無上限增長。
    QHash<QObject*, Entry> m_entries;

    // 可變快取：查詢方法為 const，故需 mutable
    mutable QHash<QString, std::shared_ptr<QSvgRenderer>> m_renderers;
    mutable QCache<TintKey, QPixmap> m_tintCache;
};

inline size_t qHash(const ThemeIcons::TintKey& key, uint seed)
{
    return qHash(key.deviceSize, seed)
         ^ qHash(key.color.rgba(), seed)
         ^ qHash(key.path, seed)
         ^ qHash(key.dpr, seed);
}

/**
 * @class ThemeFileIconProvider
 * @brief 讓 QFileSystemModel 使用主題感知的檔案/資料夾圖標
 *
 * 預設的 FileIconProvider 回傳系統圖標，在應用程式的淺色主題下常因對比不足
 * 而難以辨識。此提供者改為回傳 ThemeIcons 重新著色過的 SVG，
 * 確保在 light / dark / high-contrast 下都能清楚可見。
 *
 * 具體選哪一個 SVG 由 FileIcons 決定：它依副檔名、檔名與目錄名回傳語言圖標
 * 或專用圖標，與分頁列共用同一份對應規則。
 *
 * 因為對應結果數量有限（每種語言與檔案類別一個），這一層記憶化仍然划算：
 * 檔案樹的資料列數再多，同一類型也只需建立一次 QIcon。
 */
class ThemeFileIconProvider : public QFileIconProvider
{
public:
    using QFileIconProvider::QFileIconProvider;

    QIcon icon(IconType type) const override;
    QIcon icon(const QFileInfo& info) const override;

private:
    QIcon memoized(const QString& path) const;

    mutable QHash<QString, QIcon> m_cache;
    mutable QColor m_cacheColor;
    mutable qreal m_cacheDpr = 0.0;
};

#endif // THEMEICONS_H
