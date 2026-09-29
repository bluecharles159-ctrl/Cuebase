#include <QApplication>

#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Cuebase");
    QApplication::setOrganizationName("Cuebase");
    QApplication::setApplicationVersion(QStringLiteral(CUEBASE_VERSION));

    MainWindow window;
    window.show();
    return app.exec();
}
