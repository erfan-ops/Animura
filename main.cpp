#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QDir>
#include <QIcon>

#include <Windows.h>
#include <dwmapi.h>

#include "WallpaperController.hpp"
#include "ColorDialogHelper.hpp"

namespace {

constexpr const char* kAppName = "Animura";
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

void setupSingleInstanceServer(QLocalServer& server, QObject* rootWindow) {
    server.listen(kServerName);

    QObject::connect(&server, &QLocalServer::newConnection, [&server, rootWindow]() {
        std::unique_ptr<QLocalSocket> client(server.nextPendingConnection());
        if (!client) return;

        client->waitForReadyRead();
        const QByteArray msg = client->readAll();

        if (msg == "show" && rootWindow)
            QMetaObject::invokeMethod(rootWindow, "showMainWindow");

        client->disconnectFromServer();
        });
}

void setupTrayIcon(QApplication& app, QObject* rootWindow) {
    auto* trayIcon = new QSystemTrayIcon(QIcon(kIconPath), &app);
    auto* trayMenu = new QMenu();

    QAction* openAction = trayMenu->addAction("Open Animura");
    QAction* quitAction = trayMenu->addAction("Quit");

    QObject::connect(openAction, &QAction::triggered, [rootWindow]() {
        if (rootWindow)
            QMetaObject::invokeMethod(rootWindow, "showMainWindow");
        });

    QObject::connect(quitAction, &QAction::triggered, &app, &QCoreApplication::quit);

    trayIcon->setToolTip("Animura Wallpaper Engine");
    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    QObject::connect(trayIcon, &QSystemTrayIcon::activated,
        [rootWindow](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger && rootWindow)
                QMetaObject::invokeMethod(rootWindow, "showMainWindow");
        });
}

QColor getWindowsAccentColor() {
    DWORD color = 0;
    BOOL opaque = 0;

    if (SUCCEEDED(DwmGetColorizationColor(&color, &opaque)))
    {
        int r = (color >> 16) & 0xFF;
        int g = (color >> 8) & 0xFF;
        int b = (color & 0xFF);
        return QColor(r, g, b);
    }
    return QColor("#0078D7");
}

} // namespace

int main(int argc, char* argv[]) {
#if defined(Q_OS_WIN) && QT_VERSION_CHECK(5, 6, 0) <= QT_VERSION && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    try {
        QApplication app(argc, argv);

        const QString lockPath = QDir::temp().absoluteFilePath(QStringLiteral("Animura.lock"));
        QLockFile instanceLock(lockPath);
        instanceLock.setStaleLockTime(0);

        if (!instanceLock.tryLock(kLockTimeoutMs)) {
            if (notifyRunningInstance())
                return 0;

            // If notify failed, continue and try to start a new instance anyway.
        }

        QQmlApplicationEngine engine;
        WallpaperController wallpaperController;
        ColorDialogHelper colorDialogHelper;

        engine.rootContext()->setContextProperty("backend", &wallpaperController);
        engine.rootContext()->setContextProperty("ColorDialogHelper", &colorDialogHelper);
        engine.rootContext()->setContextProperty("WindowsAccent", getWindowsAccentColor());

        engine.load(QUrl(QStringLiteral("qrc:/qt/qml/animura/main.qml")));
        if (engine.rootObjects().isEmpty())
            return -1;

        QObject* rootWindow = engine.rootObjects().value(0);

        QLocalServer singleInstanceServer;
        setupSingleInstanceServer(singleInstanceServer, rootWindow);

        QObject::connect(&app, &QCoreApplication::aboutToQuit,
            &wallpaperController, &WallpaperController::stopWallpaper);

        app.setQuitOnLastWindowClosed(false);
        app.setWindowIcon(QIcon(kIconPath));

        setupTrayIcon(app, rootWindow);

        return app.exec();
    }
    catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error!", MB_OK | MB_ICONERROR);
        return -1;
    }
}
