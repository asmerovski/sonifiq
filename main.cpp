#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("SonifiQ");
    app.setApplicationVersion("1.0.2");
    app.setOrganizationName("SonifiQ");
    app.setWindowIcon(QIcon(":/icons/res/icons/app.svg"));

    MainWindow w;
    w.show();
    return app.exec();
}
