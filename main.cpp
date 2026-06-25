#include "mainwindow.h"
#include "crashhandler.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    CrashHandler::install();
    
    QApplication a(argc, argv);

    a.setStyleSheet(R"(
QMainWindow, QWidget {
    border-radius: 12px;
    background-color: palette(window);
}

QGroupBox {
    border: 1px solid palette(mid);
    border-radius: 12px;
    margin-top: 12px;
    padding-top: 12px;
}

QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    padding: 0 6px;
}

QTreeView,
QTabWidget::pane,
QPlainTextEdit,
QTextEdit,
QLineEdit,
QComboBox,
QAbstractItemView,
QMenuBar,
QToolBar,
QStatusBar,
QMenu {
    border: 1px solid palette(mid);
    border-radius: 12px;
    background-color: palette(base);
}

QMenuBar {
    padding: 4px;
}

QMenuBar::item,
QMenu::item,
QToolBar::item {
    border-radius: 8px;
    padding: 4px 8px;
}

QTabBar::tab {
    border: 1px solid palette(mid);
    border-bottom-left-radius: 0px;
    border-bottom-right-radius: 0px;
    border-top-left-radius: 12px;
    border-top-right-radius: 12px;
    background-color: palette(button);
    padding: 6px 12px;
    margin-right: 2px;
}

QTabBar::tab:selected {
    background-color: palette(base);
    border-bottom-color: palette(base);
}

QTabBar::tab:hover {
    background-color: palette(light);
}

QToolButton,
QPushButton,
QDialogButtonBox > QPushButton {
    border: 1px solid palette(mid);
    border-radius: 10px;
    padding: 6px 12px;
    background-color: palette(button);
}

QToolButton:hover,
QPushButton:hover,
QDialogButtonBox > QPushButton:hover {
    background-color: palette(light);
}

QLineEdit:focus,
QComboBox:focus,
QPlainTextEdit:focus,
QTextEdit:focus,
QTreeView:focus {
    border-color: palette(highlight);
}

QScrollBar:horizontal,
QScrollBar:vertical {
    border-radius: 6px;
    background: palette(alternate-base);
}

QScrollBar::handle:horizontal,
QScrollBar::handle:vertical {
    border-radius: 6px;
    background: palette(mid);
}

QScrollBar::add-line,
QScrollBar::sub-line {
    border-radius: 6px;
    background: transparent;
}
)");

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "scriptura_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w;
    w.show();
    return a.exec();
}
