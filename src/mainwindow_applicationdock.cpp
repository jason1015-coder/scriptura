#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "applicationdock.h"
#include "plugins/api/applicationmanager.h"
#include "plugins/api/applicationinterface.h"
#include "plugins/api/applicationcontext.h"
#include "codeeditor.h"
#include "firstruninstalldialog.h"
#include "welcomemenuscreen.h"

#include <QStackedWidget>
#include <QTabBar>
#include <QResizeEvent>
#include <QDebug>
#include <QTimer>
#include <QMessageBox>
#include <QSettings>

/**
 * @file mainwindow_applicationdock.cpp
 * @brief Application Dock integration for MainWindow
 *
 * This file implements:
 * - setupApplicationDock(): Creates the floating dock and registers installable apps
 * - showApplicationTab(): Shows/hides application content in the bottom panel
 * - repositionDock(): Repositions the dock at bottom center on resize
 * - resizeEvent(): Overrides MainWindow resize to reposition the dock
 */

void MainWindow::setupApplicationDock()
{
    // ── Create Application Manager ──────────────────────────────
    m_appManager = new ApplicationManager(this);

    // Register available applications (Git, HTTP, Database, etc.)
    // These are installable apps from GitHub repos, not built-in.
    m_appManager->registerAvailableApps();

    // ── Create Application Dock (floating bottom bar) ───────────
    m_appDock = new ApplicationDock(this);
    m_appDock->setObjectName("applicationDock");

    // Position the dock at the bottom center of the main window
    QTimer::singleShot(0, this, [this]() {
        if (!m_appDock) return;

        // Float the dock at the bottom center
        m_appDock->setParent(this);
        m_appDock->raise();
        m_appDock->show();
        repositionDock();
    });

    // ── Only show installed apps in the dock ───────────────────
    // Uninstalled apps are shown via the install dialog on first run.
    // The dock only displays apps that have been installed (loaded from disk).
    auto apps = m_appManager->allApplications();
    for (const auto &app : apps) {
        if (!app.iconPath.isEmpty() && app.loaded) {
            m_appDock->addEntry(app.id, app.iconPath, app.tooltip);
        }
    }

    // ── Connect dock clicks to show/hide application tabs ───────
    connect(m_appDock, &ApplicationDock::appClicked, this, [this](const QString &appId) {
        handleDockAppClicked(appId);
    });

    // ── Connect application loaded/unloaded signals ─────────────
    // When an app is actually installed (loaded from disk), add its entry to the dock
    connect(m_appManager, &ApplicationManager::applicationLoaded,
            this, [this](const QString &id, const QString &name, const QString &iconPath) {
        if (m_appDock && !m_appDock->hasEntry(id)) {
            auto info = m_appManager->appInfo(id);
            m_appDock->addEntry(id, iconPath, info.tooltip);
        }
        // Remove from pending install list if it was there
        QSettings settings;
        QStringList pending = settings.value("apps/pendingInstall").toStringList();
        if (pending.removeAll(id) > 0) {
            settings.setValue("apps/pendingInstall", pending);
        }
    });

    connect(m_appManager, &ApplicationManager::applicationUnloaded,
            this, [this](const QString &id) {
        if (m_appDock) {
            m_appDock->removeEntry(id);
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

                // Show selected apps in dock with a pending indicator
                auto apps = m_appManager->allApplications();
                for (const auto &app : apps) {
                    if (dialog.selectedApps().contains(app.id) && !m_appDock->hasEntry(app.id)) {
                        // Add to dock with pending note in tooltip
                        QString tooltip = app.tooltip + tr(" (pending install)");
                        m_appDock->addEntry(app.id, app.iconPath, tooltip);
                    }
                }
            }

            qDebug() << "FirstRunInstallDialog: skipped?" << dialog.skipped()
                     << "selected" << dialog.selectedApps().size() << "apps";
        });
    }

    qDebug() << "ApplicationDock: initialized with" << m_appDock->entryCount() << "entries";
}

void MainWindow::repositionDock()
{
    if (!m_appDock) return;
    int dockW = m_appDock->width();
    int dockH = m_appDock->height();
    int x = (width() - dockW) / 2;
    int y = height() - dockH - 12; // 12px margin from bottom
    m_appDock->move(x, y);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    repositionDock();
}

void MainWindow::handleDockAppClicked(const QString &appId)
{
    // All dock apps use the ApplicationManager
    if (!m_appManager) return;

    QWidget *appWidget = m_appManager->widget(appId);
    if (!appWidget) {
        // App is not installed — show an install prompt with the GitHub repo
        auto info = m_appManager->appInfo(appId);
        QString repo = info.githubRepo.isEmpty()
            ? tr("(URL not configured)")
            : info.githubRepo;
        QMessageBox::information(this, tr("App Not Installed"),
            tr("The \"%1\" application is not installed yet.\n\n"
               "To install it, open the Plugin Manager from "
               "the settings menu and use the GitHub URL:\n%2")
            .arg(info.name.isEmpty() ? appId : info.name)
            .arg(repo));
        return;
    }

    QString tabTitle = m_appManager->tabTitle(appId);
    int tabIndex = -1;
    for (int i = 0; i < bottomPanelTabs->count(); ++i) {
        if (bottomPanelTabs->tabText(i) == tabTitle) {
            tabIndex = i;
            break;
        }
    }

    if (tabIndex < 0) {
        tabIndex = bottomPanelTabs->addTab(tabTitle);
        bottomPanelStack->addWidget(appWidget);
    }

    bool isCurrentlyActive = (bottomPanelTabs->currentIndex() == tabIndex &&
                               ui->bottomPanelContainer->isVisible());

    if (isCurrentlyActive) {
        ui->bottomPanelContainer->hide();
        bottomPanelStack->widget(tabIndex)->hide();
        m_activeDockAppId.clear();
        if (m_appDock) m_appDock->setActiveApp(QString());
    } else {
        ui->bottomPanelContainer->show();
        bottomPanelTabs->setCurrentIndex(tabIndex);
        bottomPanelStack->setCurrentIndex(tabIndex);
        bottomPanelStack->widget(tabIndex)->show();
        m_activeDockAppId = appId;
        if (m_appDock) m_appDock->setActiveApp(appId);
        if (m_windowAnimator) {
            m_windowAnimator->animatePanelSlide(ui->bottomPanelContainer, 200);
        }
    }
}
