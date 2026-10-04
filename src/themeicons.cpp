#include "themeicons.h"
#include "thememanager.h"
#include "fileicons.h"

#include <QApplication>
#include <QAbstractButton>
#include <QAction>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QImage>
#include <QFileInfo>
#include <QSvgRenderer>

#include <atomic>

namespace {
// 每次真正光柵化 SVG 就遞增，供測試驗證快取命中率。
std::atomic<int> g_rasterizations{0};
} // namespace

ThemeIcons* ThemeIcons::instance()
{
    static ThemeIcons s_instance;
    return &s_instance;
}

int ThemeIcons::rasterizationCount()
{
    return g_rasterizations.load(std::memory_order_relaxed);
}

ThemeIcons::ThemeIcons(QObject* parent)
    : QObject(parent)
    // 以「填滿所有實際會用到的組合」為上限：20 個圖示 × 幾種尺寸 × 幾種
    // 角色色，遠低於預設值；主題切換時整池清空即可，不需要 LRU 逐出。
    , m_tintCache(4096)
{
}

void ThemeIcons::setThemeManager(const ThemeManager *mgr)
{
    m_themeManager = mgr;
}

QColor ThemeIcons::colorForRole(Role role) const
{
    // Svg role uses the theme's dedicated SVG colour (more subtle than text)
    if (role == Role::Svg && m_themeManager) {
        return m_themeManager->svgColor();
    }

    const QPalette palette = qApp ? qApp->palette() : QPalette();
    switch (role) {
    case Role::Selected:
        return palette.color(QPalette::HighlightedText);
    case Role::Disabled:
        return palette.color(QPalette::Disabled, QPalette::WindowText);
    case Role::Accent:
        return palette.color(QPalette::Highlight);
    case Role::Svg:
        // Fallback: use a slightly dimmed window text
        return palette.color(QPalette::WindowText).darker(140);
    case Role::Normal:
    default:
        return palette.color(QPalette::WindowText);
    }
}

std::shared_ptr<QSvgRenderer> ThemeIcons::rendererFor(const QString& path) const
{
    const auto cached = m_renderers.constFind(path);
    if (cached != m_renderers.constEnd())
        return cached.value();

    std::shared_ptr<QSvgRenderer> renderer;
    {
        auto r = std::make_shared<QSvgRenderer>();
        if (r->load(path)) {
            renderer = r;
        }
    }
    // 失敗也記錄 (nullptr)，避免每次都重試同一個壞路徑。
    m_renderers.insert(path, renderer);
    return renderer;
}

QPixmap ThemeIcons::tintPixmap(const QString& path, const QColor& color,
                               int logicalSize) const
{
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    const int deviceSize = qMax(1, qRound(logicalSize * dpr));

    const TintKey key{path, deviceSize, color, dpr};
    // QCache 以指標儲存並在逐出時自行 delete；nullptr 即等同未命中。
    if (const QPixmap* cached = m_tintCache.object(key))
        return *cached;

    const std::shared_ptr<QSvgRenderer> renderer = rendererFor(path);
    if (!renderer)
        return QPixmap();

    // 單一 QImage 內完成「光柵化 + 著色」：先把 SVG 以正常模式畫進透明畫布，
    // 再以 SourceIn 換色。
    //
    // 合成模式是關鍵：SourceIn 的語意是「保留 destination 的 alpha、套用
    // source 的顏色」— 這正是「用 SVG 的 alpha 當形狀遮罩、顏色換成目標色」。
    // 反過來用 DestinationIn（保留 destination 顏色、套 source alpha）不行：
    // Qt 的 raster engine 在 source 是 QSvgRenderer 這種複雜繪製器時不會把
    // destination 未被覆蓋處的 alpha 降為 0，整張圖會變成不透明色塊。
    //
    // 原檔是 currentColor、#888888 還是 fill="white" 都不影響輸出，因為 SVG
    // 的顏色在換色那一步就被丟棄了；邊緣半透明的像素則得到半透明的目標色。
    //
    // 專案內的 SVG 都只宣告 viewBox 而沒有 width/height，所以不能依賴
    // QSvgRenderer::defaultSize()，必須顯式指定渲染範圍。
    //
    // QSvgRenderer::render() 會把 viewBox 直接對應到目標矩形、不保持比例，
    // 因此這裡先依 viewBox 的長寬算出「等比縮放並置中」的目標矩形：
    // 語言圖示中副檔名較長的檔案（html、scala…）刻意使用較寬的 viewBox，
    // 若被拉伸就會變形；其餘正方形圖示的 viewBox 比例為 1，結果與舊行為一致。
    QImage image(deviceSize, deviceSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        QRectF target(0, 0, deviceSize, deviceSize);
        const QRectF viewBox = renderer->viewBoxF();
        if (viewBox.width() > 0.0 && viewBox.height() > 0.0) {
            const qreal scale = qMin(target.width() / viewBox.width(),
                                    target.height() / viewBox.height());
            const QSizeF drawn(viewBox.width() * scale, viewBox.height() * scale);
            target = QRectF((target.width() - drawn.width()) / 2.0,
                            (target.height() - drawn.height()) / 2.0,
                            drawn.width(), drawn.height());
        }
        renderer->render(&painter, target);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(image.rect(), color);
    }
    // 標記實際解析度，QIcon 才能在高 DPI 螢幕上取到正確的圖樣而不必放大。
    image.setDevicePixelRatio(dpr);

    QPixmap pixmap = QPixmap::fromImage(image);
    m_tintCache.insert(key, new QPixmap(pixmap));
    g_rasterizations.fetch_add(1, std::memory_order_relaxed);
    return pixmap;
}

QIcon ThemeIcons::icon(const QString& path, Role role, int size) const
{
    return icon(path, colorForRole(role), size);
}

QPixmap ThemeIcons::pixmap(const QString& path, Role role, int size) const
{
    return tintPixmap(path, colorForRole(role), size);
}

QIcon ThemeIcons::icon(const QString& path, const QColor& color, int size) const
{
    const QPixmap pm = tintPixmap(path, color, size);
    if (pm.isNull()) {
        // 降級：直接以資源路徑建立原始圖標
        return QIcon(path);
    }

    QIcon result;
    // pixmap 自帶 devicePixelRatio，QIcon 會據此歸檔到正確的解析度。
    result.addPixmap(pm, QIcon::Normal);
    return result;
}

QPixmap ThemeIcons::pixmap(const QString& path, const QColor& color, int size) const
{
    return tintPixmap(path, color, size);
}

QIcon ThemeIcons::statefulIcon(const QString& path, int size) const
{
    QIcon result;
    const QPixmap normal = tintPixmap(path, colorForRole(Role::Normal), size);
    const QPixmap selected = tintPixmap(path, colorForRole(Role::Selected), size);
    if (normal.isNull())
        return QIcon(path);
    result.addPixmap(normal, QIcon::Normal);
    // QIcon::Selected is the mode QTabBar and QTreeView ask for on a
    // :selected row, so the glyph stays readable on the highlight fill.
    if (!selected.isNull())
        result.addPixmap(selected, QIcon::Selected);
    return result;
}

void ThemeIcons::setIcon(QAbstractButton* button, const QString& path, Role role, int size)
{
    if (!button) return;
    button->setIcon(icon(path, role, size));
    m_entries.insert(button, Entry{button, path, role, size});
    pruneDeadEntries();
}

void ThemeIcons::setIcon(QAction* action, const QString& path, Role role, int size)
{
    if (!action) return;
    action->setIcon(icon(path, role, size));
    m_entries.insert(action, Entry{action, path, role, size});
    pruneDeadEntries();
}

void ThemeIcons::setIcon(QLabel* label, const QString& path, Role role, const QSize& size)
{
    if (!label) return;
    const int extent = size.isEmpty() ? DefaultSize : qMax(size.width(), size.height());
    label->setPixmap(tintPixmap(path, colorForRole(role), extent));
    m_entries.insert(label, Entry{label, path, role, extent});
    pruneDeadEntries();
}

void ThemeIcons::pruneDeadEntries()
{
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (it->target.isNull())
            it = m_entries.erase(it);
        else
            ++it;
    }
}

void ThemeIcons::recolorAll()
{
    // 顏色全變，著色快取整池失效；SVG 解析結果與主題無關，保留。
    m_tintCache.clear();
    pruneDeadEntries();

    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        // 取出副本：setIcon() 觸發樣式重算，理論上可能引發重入。
        const Entry entry = it.value();
        QObject* target = entry.target.data();
        if (!target)
            continue;

        const QColor color = colorForRole(entry.role);
        if (auto* btn = qobject_cast<QAbstractButton*>(target)) {
            btn->setIcon(icon(entry.path, entry.role, entry.size));
        } else if (auto* action = qobject_cast<QAction*>(target)) {
            action->setIcon(icon(entry.path, entry.role, entry.size));
        } else if (auto* label = qobject_cast<QLabel*>(target)) {
            label->setPixmap(tintPixmap(entry.path, color, entry.size));
        }
    }
}

QIcon ThemeFileIconProvider::memoized(const QString& path) const
{
    ThemeIcons* ti = ThemeIcons::instance();
    const QColor color = ti->colorForRole(ThemeIcons::Role::Normal);
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;

    // 主题色或 dpr 變了就整批失效 — 兩者都會改變像素結果。
    if (m_cacheColor != color || m_cacheDpr != dpr) {
        m_cache.clear();
        m_cacheColor = color;
        m_cacheDpr = dpr;
    }

    const auto cached = m_cache.constFind(path);
    if (cached != m_cache.constEnd())
        return cached.value();

    // statefulIcon, not icon: rows drawn on the highlight fill need the
    // highlighted-text colour or the glyph disappears into the background.
    // FileTreeSize, not DefaultSize: the view's iconSize is pinned to the same
    // constant, so the size requested at paint time is an exact cache hit and
    // the SVG is never scaled up from a 16px rasterisation.
    const QIcon result = ti->statefulIcon(path, ThemeIcons::FileTreeSize);
    m_cache.insert(path, result);
    return result;
}

QIcon ThemeFileIconProvider::icon(QFileIconProvider::IconType type) const
{
    if (type == QFileIconProvider::Folder) {
        return memoized(":/icons/folder.svg");
    }
    return memoized(":/icons/file.svg");
}

QIcon ThemeFileIconProvider::icon(const QFileInfo& info) const
{
    // FileIcons owns the type decision so the tree, the tabs and any future view
    // cannot drift apart on which glyph a path gets. Reuse the QFileInfo we were
    // handed rather than letting forPath() stat the path again: the file-system
    // model already knows whether this row is a directory. It is a lookup in a
    // handful of static QHashes, so this stays cheap for every painted row.
    const QString baseName = info.fileName();
    return memoized(info.isDir() ? FileIcons::forDirectoryName(baseName)
                                 : FileIcons::forFileName(baseName));
}
