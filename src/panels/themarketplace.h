#ifndef THEMEMARKETPLACE_H
#define THEMEMARKETPLACE_H

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QJsonArray>

/**
 * Theme Marketplace widget for browsing and installing community themes.
 * Features:
 * - Browse available themes from a remote registry
 * - Preview theme colors before applying
 * - One-click install
 * - Show installed themes
 */
class ThemeMarketplaceWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ThemeMarketplaceWidget(QWidget *parent = nullptr);

    void loadThemes(const QJsonArray &themes);
    void loadBuiltinThemes();
    void refreshThemes();

signals:
    void themeInstalled(const QString &themeName);
    void themePreviewRequested(const QJsonObject &theme);

private slots:
    void onThemeSelected(int index);
    void onInstallClicked();
    void onPreviewClicked();
    void onRefreshClicked();

private:
    void setupUI();
    void updatePreview(const QJsonObject &theme);

    QListWidget *m_themeList;
    QLabel *m_previewLabel;
    QLabel *m_detailLabel;
    QLabel *m_statusLabel;
    QPushButton *m_installBtn;
    QPushButton *m_previewBtn;
    QPushButton *m_refreshBtn;
    QJsonArray m_themes;
};

#endif // THEMEMARKETPLACE_H
