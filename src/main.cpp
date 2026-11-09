#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QUuid>
#include "ChannelDirectory.h"
#include "UiControls.h"

#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    installUiBehavior(&app);
    QApplication::setApplicationName("Cuebase");
    QApplication::setOrganizationName("Cuebase");
    QApplication::setApplicationVersion(QStringLiteral(CUEBASE_VERSION));

    // Process-level tests launch the same executable as the desktop shortcut.
    // A unique, validated organization keeps their libraries and locks isolated.
    const QUuid testProfile(qEnvironmentVariable("CUEBASE_TEST_PROFILE"));
    if(!testProfile.isNull()){
        QStandardPaths::setTestModeEnabled(true);
        QApplication::setOrganizationName("CuebaseTest-"+testProfile.toString(QUuid::WithoutBraces));
    }
    const auto mainDir=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const auto args=app.arguments();const int selector=args.indexOf("--channel");
    const auto id=selector<0?QStringLiteral("main"):args.value(selector+1);
    ChannelDirectory directory(mainDir,id);
    const auto profile=ChannelDirectory::profileName(id);
    const auto channels=directory.list();
    if(profile.isEmpty() || !channels.contains(id)){QMessageBox::warning(nullptr,"Cuebase","This channel is unavailable.");return 1;}
    QApplication::setApplicationName(profile);QApplication::setApplicationDisplayName("Cuebase");
    if(id!="main")app.setProperty("initialChannelName",channels.value(id));
    const auto channelDir=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if(!QDir().mkpath(channelDir)){QMessageBox::warning(nullptr,"Cuebase","Could not open the channel storage folder.");return 1;}
    const auto socketName=directory.windowEndpoint(id);
    QLockFile lock(QDir(channelDir).filePath("studio-window.lock"));lock.setStaleLockTime(0);
    if(!lock.tryLock(0)){
        QLocalSocket existing;existing.connectToServer(socketName);
        if(existing.waitForConnected(1000)){existing.write("show");existing.flush();existing.waitForBytesWritten(200);return 0;}
        QMessageBox::information(nullptr,"Cuebase","This channel is open or still starting. Switch to its Cuebase window.");return 1;
    }
    QLocalServer server;server.setSocketOptions(QLocalServer::UserAccessOption);QLocalServer::removeServer(socketName);
    if(!server.listen(socketName)){QMessageBox::warning(nullptr,"Cuebase","Could not open the channel window connection.");return 1;}
    MainWindow window(nullptr,&directory);
    QObject::connect(&server,&QLocalServer::newConnection,&window,[&]{
        while(auto *client=server.nextPendingConnection()){
            auto showWindow=[client,&window]{
                if(client->bytesAvailable()<4)return;
                if(client->read(4)=="show"){
                    if(window.isMinimized())window.setWindowState(window.windowState() & ~Qt::WindowMinimized);
                    window.raise();window.activateWindow();
                    client->write("ready");client->flush();
                }
                client->disconnectFromServer();
            };
            QObject::connect(client,&QLocalSocket::disconnected,client,&QObject::deleteLater);
            QObject::connect(client,&QLocalSocket::readyRead,&window,showWindow);
            showWindow();
        }
    });
    window.show();
    window.enterWorkspace();
    return app.exec();
}
