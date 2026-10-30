#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("SonifiQ");
    app.setApplicationVersion(SONIFIQ_VERSION_STRING);
    app.setOrganizationName("AppsMall");
    app.setWindowIcon(QIcon(":/icons/res/icons/app.svg"));

    MainWindow w;
    w.show();
    return app.exec();
}
