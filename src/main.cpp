#include <QApplication>
#include "mainwindow.h"
#include "thememanager.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("SonifiQ");
    app.setApplicationVersion("1.2.1");
    app.setOrganizationName("AppSMall");
    app.setWindowIcon(QIcon(":/icons/res/icons/app.svg"));

    ThemeManager::applySavedTheme();

    MainWindow w;
    w.show();
    return app.exec();
}
