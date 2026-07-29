#ifndef FIRSTRUNINSTALLDIALOG_H
#define FIRSTRUNINSTALLDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QCheckBox>
#include <QFrame>

class ApplicationManager;

/**
 * @file firstruninstalldialog.h
 * @brief First-run dialog that prompts the user to install available apps
 *
 * On the very first launch of Scriptura, this dialog appears and shows
 * all registered applications that can be installed from GitHub repositories.
 * The user can select which apps to install (or skip all).
 * GitHub URLs are placeholders and should be replaced with actual repos.
 */
class FirstRunInstallDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FirstRunInstallDialog(ApplicationManager *appManager, QWidget *parent = nullptr);

    QStringList selectedApps() const { return m_selectedApps; }
    bool skipped() const { return m_skipped; }

private:
    void setupUI();
    void onInstallClicked();
    void onSkipClicked();

    ApplicationManager *m_appManager;
    QList<QCheckBox*> m_appCheckBoxes;
    QList<QString> m_appIds;
    QStringList m_selectedApps;
    bool m_skipped = false;
};

#endif // FIRSTRUNINSTALLDIALOG_H
