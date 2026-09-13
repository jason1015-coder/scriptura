#include "qt_bridge.h"
#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QSettings>
#include <QIcon>
#include <QMainWindow>
#include <memory>

static QApplication* g_qt_app = nullptr;
static void* g_rust_app = nullptr;

extern "C" {

void* cpp_qt_create_app(int argc, char** argv) {
    g_qt_app = new QApplication(argc, argv);
    return g_qt_app;
}

void cpp_qt_destroy_app(void* app) {
    if (app == nullptr) return;
    delete static_cast<QApplication*>(app);
    g_qt_app = nullptr;
}

void cpp_qt_set_org_name(void* app, const char* name) {
    if (app == nullptr || name == nullptr) return;
    QApplication::setOrganizationName(QString::fromUtf8(name));
}

void cpp_qt_set_app_name(void* app, const char* name) {
    if (app == nullptr || name == nullptr) return;
    QApplication::setApplicationName(QString::fromUtf8(name));
}

void cpp_qt_set_style(void* app, const char* style) {
    if (app == nullptr || style == nullptr) return;
    QApplication::setStyle(QString::fromUtf8(style));
}

void cpp_qt_set_window_icon(void* app) {
    if (g_qt_app == nullptr) return;
    g_qt_app->setWindowIcon(QIcon(":/icon.png"));
}

void cpp_qt_load_translations(void* app) {
    if (g_qt_app == nullptr) return;
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString& locale : uiLanguages) {
        const QString baseName = "scriptura_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            g_qt_app->installTranslator(&translator);
            break;
        }
    }
}

void cpp_qt_load_theme(void* app, int theme_id, int mode) {
    Q_UNUSED(app)
    Q_UNUSED(theme_id)
    Q_UNUSED(mode)
}

void* cpp_qt_create_main_window(void* app, void* rust_app, const char* project, const char** files, int file_count) {
    Q_UNUSED(app)
    g_rust_app = rust_app;
    QString initialProject;
    QStringList initialFiles;
    if (project != nullptr) {
        initialProject = QString::fromUtf8(project);
    }
    if (files != nullptr) {
        for (int i = 0; i < file_count; ++i) {
            if (files[i] != nullptr) {
                initialFiles.append(QString::fromUtf8(files[i]));
            }
        }
    }
    QMainWindow* mainWindow = new QMainWindow();
    mainWindow->setWindowTitle(QApplication::applicationName());
    return mainWindow;
}

void cpp_qt_show_window(void* app, void* window) {
    Q_UNUSED(app)
    if (window == nullptr || g_qt_app == nullptr) return;
    static_cast<QMainWindow*>(window)->show();
}

int cpp_qt_run_event_loop(void* app) {
    Q_UNUSED(app)
    if (g_qt_app == nullptr) return 0;
    return g_qt_app->exec();
}

}
