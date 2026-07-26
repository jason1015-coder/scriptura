#include "themeicons.h"
#include "thememanager.h"

#include <QApplication>
#include <QAbstractButton>
#include <QAction>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QImageReader>
#include <QFileInfo>

ThemeIcons* ThemeIcons::instance()
{
    static ThemeIcons s_instance;
    return &s_instance;
}

ThemeIcons::ThemeIcons(QObject* parent)
    : QObject(parent)
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

// 以目標尺寸渲染 SVG (透過 Qt 的 SVG 圖像外掛，不需連結 Svg 開發模組)。
// 若無法在目標尺寸渲染，則以內部尺寸渲染後平滑縮放。
QPixmap renderSvg(const QString& path, int size)
{
    if (size <= 0) {
        return QPixmap();
    }

    QImageReader reader(path);
    if (reader.canRead()) {
        reader.setScaledSize(QSize(size, size));
        const QImage img = reader.read();
        if (!img.isNull()) {
            return QPixmap::fromImage(img);
        }
    }

    // 降級：直接以資源路徑載入 (Qt 會在內部尺寸渲染 SVG)
    QPixmap pm(path);
    if (pm.isNull()) {
        return QPixmap();
    }
    return pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QPixmap tintedPixmap(const QString& path, const QColor& color, int size)
{
    const QPixmap shape = renderSvg(path, size);
    if (shape.isNull()) {
        return QPixmap();
    }

    // 用「目標顏色」作為底色，再以 DestinationIn 用 SVG 的 alpha 形狀遮罩，
    // 得到「形狀不變、顏色 = 目前主題前景色」的圖標。
    QPixmap out(size, size);
    out.fill(color);
    {
        QPainter p(&out);
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        p.drawPixmap(0, 0, shape);
    }
    return out;
}

QIcon ThemeIcons::makeIcon(const QString& path, const QColor& color) const
{
    // 提供多種尺寸以在各縮放下保持清晰
    static const QList<int> sizes = {16, 18, 20, 24, 32, 48, 64};
    QIcon result;
    for (int s : sizes) {
        const QPixmap pm = tintedPixmap(path, color, s);
        if (!pm.isNull()) {
            result.addPixmap(pm, QIcon::Normal);
            // 停用態：用較暗的顏色
            const QPixmap disabled = tintedPixmap(path, color.darker(160), s);
            if (!disabled.isNull()) {
                result.addPixmap(disabled, QIcon::Disabled);
            }
        }
    }
    if (result.isNull()) {
        // 降級：直接以資源路徑建立原始圖標
        return QIcon(path);
    }
    return result;
}

QIcon ThemeIcons::icon(const QString& path, Role role) const
{
    return makeIcon(path, colorForRole(role));
}

QIcon ThemeIcons::rawIcon(const QString& path) const
{
    return QIcon(path);
}

void ThemeIcons::setIcon(QAbstractButton* button, const QString& path, Role role)
{
    if (!button) return;
    button->setIcon(icon(path, role));
    Entry e;
    e.target = button;
    e.path = path;
    e.role = role;
    e.size = QSize();
    m_entries.append(e);
}

void ThemeIcons::setIcon(QAction* action, const QString& path, Role role)
{
    if (!action) return;
    action->setIcon(icon(path, role));
    Entry e;
    e.target = action;
    e.path = path;
    e.role = role;
    e.size = QSize();
    m_entries.append(e);
}

void ThemeIcons::setIcon(QLabel* label, const QString& path, Role role, const QSize& size)
{
    if (!label) return;
    const int extent = size.isEmpty() ? 16 : qMax(size.width(), size.height());
    label->setPixmap(tintedPixmap(path, colorForRole(role), extent));
    Entry e;
    e.target = label;
    e.path = path;
    e.role = role;
    e.size = (size.isEmpty() ? QSize(extent, extent) : size);
    m_entries.append(e);
}

void ThemeIcons::recolorAll()
{
    // 反向迭代以便安全移除失效條目
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        const Entry& e = m_entries.at(i);
        QObject* target = e.target.data();
        if (!target) {
            m_entries.removeAt(i);
            continue;
        }

        const QColor color = colorForRole(e.role);

        if (auto* btn = qobject_cast<QAbstractButton*>(target)) {
            btn->setIcon(makeIcon(e.path, color));
        } else if (auto* action = qobject_cast<QAction*>(target)) {
            action->setIcon(makeIcon(e.path, color));
        } else if (auto* label = qobject_cast<QLabel*>(target)) {
            const int extent = e.size.isEmpty() ? 16 : qMax(e.size.width(), e.size.height());
            label->setPixmap(tintedPixmap(e.path, color, extent));
        } else {
            m_entries.removeAt(i);
        }
    }
}

QIcon ThemeFileIconProvider::icon(QFileIconProvider::IconType type) const
{
    if (type == QFileIconProvider::Folder) {
        return ThemeIcons::instance()->icon(":/icons/folder.svg");
    }
    return ThemeIcons::instance()->icon(":/icons/file.svg");
}

QIcon ThemeFileIconProvider::icon(const QFileInfo& info) const
{
    if (info.isDir()) {
        return ThemeIcons::instance()->icon(":/icons/folder.svg");
    }
    return ThemeIcons::instance()->icon(":/icons/file.svg");
}
