#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "plugins/api/applicationmanager.h"
#include "plugins/api/applicationinterface.h"
#include "plugins/api/applicationcontext.h"
#include "codeeditor.h"
#include "firstruninstalldialog.h"
#include "welcomemenuscreen.h"

#include <QDebug>
#include <QTimer>
#include <QSettings>
#include <QStandardPaths>
#include <QCoreApplication>

/**
 * @file mainwindow_applicationdock.cpp
 * @brief Applications (installable apps) integration for MainWindow
 *
 * This file implements setupApplicationDock(): scans installed apps,
 * registers available ones, and prompts the first-run install dialog.
 * The floating application dock UI was removed (unused) — apps are
 * reachable through the plugin manager instead.
 */

void MainWindow::setupApplicationDock()
{
    // ── Create Application Manager ──────────────────────────────
    m_appManager = new ApplicationManager(this);

    // ── Scan for installed apps FIRST ───────────────────────────
    // Check the user's plugin directory for installed applications.
    QString pluginDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugins";
    m_appManager->scanDirectory(pluginDir);
    m_appManager->scanDirectory(QCoreApplication::applicationDirPath() + "/plugins");

    // Register available applications (Git, HTTP, Database, etc.)
    // These are installable apps from GitHub repos, not built-in.
    // This runs AFTER scanning so already-discovered apps get proper appDir.
    m_appManager->registerAvailableApps();

    // ── Load all discovered applications ───────────────────────
    m_appManager->loadAll();

    // ── Connect application loaded signal ──────────────────────
    // When an app is actually installed (loaded from disk), clean the
    // pending install list.
    connect(m_appManager, &ApplicationManager::applicationLoaded,
            this, [this](const QString &id, const QString &name, const QString &iconPath) {
        Q_UNUSED(name);
        Q_UNUSED(iconPath);
        // Remove from pending install list if it was there
        QSettings settings;
        QStringList pending = settings.value("apps/pendingInstall").toStringList();
        if (pending.removeAll(id) > 0) {
            settings.setValue("apps/pendingInstall", pending);
        }
    });

    // ── On first launch, prompt user to install apps from GitHub ────
    if (WelcomeMenuScreen::isFirstLaunch()) {
        QTimer::singleShot(0, this, [this]() {
            // Activate the window first so the dialog has a visible parent
            activateWindow();
            raise();

            FirstRunInstallDialog dialog(m_appManager, this);
            dialog.exec();

            // Mark as launched so this dialog doesn't show again
            WelcomeMenuScreen::markLaunched();

            // Save selected apps to settings so they appear as "pending install"
            if (!dialog.skipped()) {
                QSettings settings;
                settings.setValue("apps/pendingInstall", dialog.selectedApps());
            }

            qDebug() << "FirstRunInstallDialog: skipped?" << dialog.skipped()
                     << "selected" << dialog.selectedApps().size() << "apps";
        });
    }
}
