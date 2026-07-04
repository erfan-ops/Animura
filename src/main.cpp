/**
 * @file main.cpp
 * @brief Application entry point for the Animura live wallpaper engine.
 *
 * Sets up the Qt application shell, enforces single-instance behavior,
 * creates the WallpaperController and WebView2Host, configures the system
 * tray icon, and enters the Qt event loop.
 *
 * ## Startup Sequence
 * 1. Set DLL search directory restrictions for security.
 * 2. Create QApplication.
 * 3. Acquire QLockFile — if already locked, notify the existing instance
 *    and exit. This ensures only one Animura process runs at a time.
 * 4. Construct WallpaperController (scans ./modules/).
 * 5. Construct WebView2Host (creates QWidget window, initializes WebView2,
 *    registers NativeBridge, navigates to the React frontend).
 * 6. Connect WallpaperController::runningModuleChanged signal to
 *    WebView2Host::postMessageToJs for C++→JS state forwarding.
 * 7. Set up QLocalServer to receive "show" messages from secondary
 *    launch attempts.
 * 8. Configure the system tray icon with Open/Quit actions.
 * 9. Enter the Qt event loop (app.exec()).
 *
 * ## Tray Behavior
 * The app lives in the system tray — closing the window hides it rather
 * than quitting. "Quit" from the tray menu triggers
 * QCoreApplication::quit(), which fires aboutToQuit → stopWallpaper.
 */

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

/** Name used for the QLocalServer/QLocalSocket single-instance channel. */
constexpr const char* kServerName = "AnimuraInstance";

/** Qt resource path to the application icon. */
constexpr const char* kIconPath = ":/icons/icon.ico";

/** Timeout in milliseconds for single-instance socket connection attempts. */
constexpr int         kLockTimeoutMs = 100;

/**
 * @brief Attempts to notify the already-running Animura instance to show
 *        its window.
 *
 * Connects to the named QLocalServer and sends a "show" message.
 * Called when the lock file is already held, indicating another instance
 * is running.
 *
 * @return true if the running instance was successfully notified.
 */
bool tryNotifyRunningInstance() {
    QLocalSocket socket;
    socket.connectToServer(kServerName);

    if (!socket.waitForConnected(kLockTimeoutMs))
        return false;

    socket.write("show");
    socket.flush();
    socket.waitForBytesWritten();
    return true;
}

/**
 * @brief Creates and configures the system tray icon.
 *
 * The tray icon provides:
 * - "Open Animura" — shows and raises the main window.
 * - "Quit" — triggers QCoreApplication::quit().
 * - Double-click — shows the main window.
 *
 * @param app        The QApplication instance (parent for the tray icon).
 * @param mainWindow The main QWidget window to show/raise on activation.
 */
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
        // Restrict DLL search to system directories for security.
        // Module DLL loading uses explicit LoadLibraryEx flags to allow
        // the module's own directory.
        SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

        QApplication app(argc, argv);

        // ── Single-instance enforcement ──
        const QString lockPath = QDir::temp().absoluteFilePath(QStringLiteral("Animura.lock"));
        QLockFile instanceLock(lockPath);
        instanceLock.setStaleLockTime(0);

        if (!instanceLock.tryLock(kLockTimeoutMs)) {
            if (tryNotifyRunningInstance())
                return 0;
        }

        // ── Core components ──
        WallpaperController wallpaperController;

        // Create WebView2 host window (replaces QQmlApplicationEngine)
        WebView2Host webViewHost(&wallpaperController);
        QWidget* mainWindow = webViewHost.widget();

        // Bridge: forward WallpaperController signals to React via WebView2.
        // When a module starts or stops, the React UI updates its button states.
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

        // ── Single-instance server ──
        // Listens for "show" messages from secondary launch attempts.
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

        // The app lives in the tray — closing the window hides it, does not quit.
        app.setQuitOnLastWindowClosed(false);
        app.setWindowIcon(QIcon(kIconPath));

        setupTrayIcon(app, mainWindow);

        return app.exec();
    }
    catch (const std::exception& e) {
        // Top-level exception handler — shows a message box for fatal errors
        // (e.g., "No modules were found", "Modules folder not found!").
        MessageBoxA(nullptr, e.what(), "Error!", MB_OK | MB_ICONERROR);
        return -1;
    }
}
