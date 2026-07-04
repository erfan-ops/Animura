#pragma once

#include <QObject>
#include <QWidget>
#include <QString>
#include <memory>

struct ICoreWebView2Controller;
struct ICoreWebView2;
struct ICoreWebView2Environment;
class NativeBridge;
class WallpaperController;

/**
 * @brief Hosts a Microsoft Edge WebView2 control inside a Qt QWidget window.
 *
 * WebView2Host replaces the previous QQmlApplicationEngine with a Chromium-based
 * web view that renders the React + TypeScript frontend. It wraps the low-level
 * WebView2 COM API in a Qt-friendly interface.
 *
 * ## Responsibilities
 * - Create a top-level QWidget window (purple background, centered on screen).
 * - Initialize the WebView2 environment and controller asynchronously.
 * - Configure settings (scripts enabled, context menus disabled,
 *   dev tools gated by `kDevMode`).
 * - Register `NativeBridge` as a COM host object reachable from JS at
 *   `window.chrome.webview.hostObjects.nativeBridge`.
 * - Set up virtual host mappings:
 *   - `animura.app` → `frontend/dist/` (production React build).
 *   - `animura.modules` → `modules/` (wallpaper preview images).
 * - Handle navigation: dev server (`http://localhost:5173`) or production
 *   build (`https://animura.app/index.html`).
 * - Forward C++ signals to JavaScript via `PostWebMessageAsJson`.
 *
 * ## Threading
 * WebView2 callbacks arrive on arbitrary threads. NativeBridge marshals
 * all JS→C++ calls to the Qt main thread. `PostWebMessageAsJson` is
 * thread-safe and can be called from any thread.
 *
 * @see NativeBridge for the COM IDispatch bridge registered as a host object.
 */
class WebView2Host : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs the WebView2 host window.
     *
     * Creates a `WebView2Widget` (custom QWidget subclass that hides to tray
     * instead of closing), sizes it to 1280×720, centers it on the primary
     * screen, and starts the asynchronous WebView2 initialization.
     *
     * @param controller The WallpaperController exposed to JS via NativeBridge.
     * @param parent     Qt parent object (default nullptr).
     */
    explicit WebView2Host(WallpaperController* controller, QObject* parent = nullptr);

    /**
     * @brief Destroys the WebView2 host.
     *
     * Closes and releases the WebView2 controller, WebView, environment,
     * and NativeBridge COM pointers. Destroys the QWidget window.
     */
    ~WebView2Host() override;

    /**
     * @brief Returns the QWidget window that contains the WebView2 control.
     *
     * Used by main.cpp to integrate with the system tray (show/raise/activate)
     * and single-instance server.
     *
     * @return Pointer to the top-level QWidget.
     */
    QWidget* widget() const { return m_window; }

    /**
     * @brief Sends a JSON message to the JavaScript running in the WebView2.
     *
     * Used to forward C++ signals (e.g., `runningModuleChanged`) to the React
     * frontend. The React app listens via
     * `window.chrome.webview.addEventListener('message', ...)`.
     *
     * Safe to call from any thread — `PostWebMessageAsJson` is thread-safe.
     *
     * @param json A JSON string to deliver to the JS `message` event listener.
     */
    void postMessageToJs(const QString& json);

signals:
    /** Emitted when the QWidget window is destroyed. */
    void windowClosing();

private:
    /**
     * @brief Initializes the WebView2 environment asynchronously.
     *
     * Creates the WebView2 environment, controller, and CoreWebView2.
     * Registers NativeBridge as a host object, configures virtual host
     * mappings for module preview images, and sets up web message and
     * navigation-completed event handlers.
     *
     * Called once from the constructor.
     */
    void initWebView2();

    /**
     * @brief Navigates the WebView2 to the React application.
     *
     * In dev mode (`kDevMode = true`): navigates to `http://localhost:5173`.
     * In production: maps `animura.app` virtual host to the `frontend/dist/`
     * folder and navigates to `https://animura.app/index.html`.
     *
     * Falls back to file-based navigation if the ICoreWebView2_3 interface
     * (required for virtual host mapping) is unavailable.
     */
    void navigateToApp();

    /** The top-level Qt window containing the WebView2 control. */
    QWidget* m_window{ nullptr };

    /** The WallpaperController exposed to JS via NativeBridge. Non-owning. */
    WallpaperController* m_controller{ nullptr };

    /** The COM IDispatch bridge. COM reference-counted. */
    NativeBridge* m_bridge{ nullptr };

    // ── WebView2 COM pointers (raw — released manually in destructor) ──

    /** WebView2 environment (shared across all controls in the process). */
    ICoreWebView2Environment* m_env{ nullptr };

    /** Controller for the WebView2 control (handles bounds, visibility, parent HWND). */
    ICoreWebView2Controller* m_webViewController{ nullptr };

    /** The core WebView2 web content control (navigation, settings, script injection). */
    ICoreWebView2* m_webView{ nullptr };

    /** Set to true when the first navigation completes successfully. */
    bool m_initialized{ false };

    /**
     * Event registration tokens for WebView2 events.
     * Stored as 64-bit values (EventRegistrationToken is an 8-byte struct).
     * Only stored for documentation; not currently used to deregister.
     */
    long long m_navCompletedToken{ 0 };
    long long m_webMessageToken{ 0 };
};
