#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QDir>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>

#include <Windows.h>

#include "WallpaperController.hpp"
#include "WebView2Host.hpp"

namespace {

constexpr const char* kServerName = "AnimuraInstance";
constexpr const char* kIconPath = ":/icons/icon.ico";
constexpr int         kLockTimeoutMs = 100;

bool notifyRunningInstance() {
    QLocalSocket socket;
    socket.connectToServer(kServerName);

    if (!socket.waitForConnected(kLockTimeoutMs))
        return false;

    socket.write("show");
    socket.flush();
    socket.waitForBytesWritten();
    return true;
}

void setupTrayIcon(QApplication& app, QWidget* mainWindow) {
    auto* trayIcon = new QSystemTrayIcon(QIcon(kIconPath), &app);
    auto* trayMenu = new QMenu();

    QAction* openAction = trayMenu->addAction("Open Animura");
    QAction* quitAction = trayMenu->addAction("Quit");

    QObject::connect(openAction, &QAction::triggered, [mainWindow]() {
        if (mainWindow) {
            mainWindow->show();
            mainWindow->raise();
            mainWindow->activateWindow();
        }
    });

    QObject::connect(quitAction, &QAction::triggered, &app, &QCoreApplication::quit);

    trayIcon->setToolTip("Animura Wallpaper Engine");
    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    QObject::connect(trayIcon, &QSystemTrayIcon::activated,
        [mainWindow](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger && mainWindow) {
                mainWindow->show();
                mainWindow->raise();
                mainWindow->activateWindow();
            }
        });
}

} // namespace

int main(int argc, char* argv[]) {
#if defined(Q_OS_WIN) && QT_VERSION_CHECK(5, 6, 0) <= QT_VERSION && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    try {
        SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

        QApplication app(argc, argv);

        const QString lockPath = QDir::temp().absoluteFilePath(QStringLiteral("Animura.lock"));
        QLockFile instanceLock(lockPath);
        instanceLock.setStaleLockTime(0);

        if (!instanceLock.tryLock(kLockTimeoutMs)) {
            if (notifyRunningInstance())
                return 0;
        }

        WallpaperController wallpaperController;

        // Create WebView2 host window (replaces QQmlApplicationEngine)
        WebView2Host webViewHost(&wallpaperController);
        QWidget* mainWindow = webViewHost.widget();

        // Bridge: forward WallpaperController signals to React via WebView2
        QObject::connect(
            &wallpaperController, &WallpaperController::runningModuleChanged,
            &webViewHost, [&webViewHost, &wallpaperController]() {
                int moduleId = wallpaperController.getRunningModuleId();
                QJsonObject msg;
                msg["type"] = QStringLiteral("runningModuleChanged");
                msg["moduleId"] = moduleId;
                QJsonDocument doc(msg);
                webViewHost.postMessageToJs(doc.toJson(QJsonDocument::Compact));
            }
        );

        // Single-instance server
        QLocalServer singleInstanceServer;
        QObject::connect(&singleInstanceServer, &QLocalServer::newConnection,
            [&singleInstanceServer, mainWindow]() {
                std::unique_ptr<QLocalSocket> client(
                    singleInstanceServer.nextPendingConnection());
                if (!client) return;
                client->waitForReadyRead();
                const QByteArray msg = client->readAll();
                if (msg == "show" && mainWindow) {
                    mainWindow->show();
                    mainWindow->raise();
                    mainWindow->activateWindow();
                }
                client->disconnectFromServer();
            });
        singleInstanceServer.listen(kServerName);

        QObject::connect(&app, &QCoreApplication::aboutToQuit,
            &wallpaperController, &WallpaperController::stopWallpaper);

        app.setQuitOnLastWindowClosed(false);
        app.setWindowIcon(QIcon(kIconPath));

        setupTrayIcon(app, mainWindow);

        return app.exec();
    }
    catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error!", MB_OK | MB_ICONERROR);
        return -1;
    }
}
