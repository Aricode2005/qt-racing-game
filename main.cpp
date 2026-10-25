#include <QApplication>
#include "GameWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("ApexVelocityRacing");
    app.setOrganizationName("AntigravityGames");

    GameWindow window;
    window.show();

    return app.exec();
}
