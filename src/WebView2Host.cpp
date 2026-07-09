/**
 * @file WebView2Host.cpp
 * @brief Implementation of the Microsoft Edge WebView2 host embedded in a
 *        Qt QWidget window.
 *
 * WebView2Host creates a QWidget window and initializes a WebView2 control
 * inside it to render the React + TypeScript frontend. It replaces the
 * previous QQmlApplicationEngine-based QML UI.
 *
 * The initialization is asynchronous: `CreateCoreWebView2EnvironmentWithOptions`
 * fires a callback when the environment is ready, which then creates the
 * controller, configures settings, registers the NativeBridge COM host
 * object, sets up virtual host mappings, and navigates to the React app.
 *
 * ## Virtual Host Mappings
 * - `animura.app` → `frontend/dist/` (production React build).
 * - `animura.modules` → `modules/` (wallpaper preview images).
 *
 * These mappings allow the React app to load resources via HTTPS URLs
 * without `file:///` restrictions, and to access module preview images
 * from a path that the host controls.
 */

#include "WebView2Host.hpp"
#include "NativeBridge.hpp"
#include "WallpaperController.hpp"

#include <QWidget>
#include <QApplication>
#include <QScreen>
#include <QDebug>
#include <QResizeEvent>
#include <QCloseEvent>

#include <WebView2.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <windows.h>
#include <ShObjIdl.h>

using Microsoft::WRL::Callback;

/**
 * @brief Local struct compatible with EventRegistrationToken.
 *
 * Avoids including winrt/EventToken.h. EventRegistrationToken is an 8-byte
 * struct; this struct provides an implicit conversion operator so it can be
 * passed wherever the SDK expects `EventRegistrationToken*`.
 */
struct LocalEventToken {
    long long value;
    operator EventRegistrationToken*() {
        return reinterpret_cast<EventRegistrationToken*>(this);
    }
};

namespace {

/** When true, navigates to the Vite dev server instead of the production build. */
constexpr bool kDevMode = false;

/** Vite dev server URL (only used when kDevMode is true). */
constexpr const wchar_t* kDevUrl = L"http://localhost:5173";

/** Virtual host name for the production React build. */
constexpr const wchar_t* kVirtualHost = L"animura.app";

/** Default window width in logical pixels. */
constexpr int kDefaultWidth = 1400;

/** Default window height in logical pixels. */
constexpr int kDefaultHeight = 720;

/**
 * @brief Resolves the absolute path to the frontend build output directory.
 *
 * Computes `<exe_dir>\frontend\dist` relative to the running executable.
 * @return Absolute path string.
 */
std::wstring getFrontendPath() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring path(exePath);
    auto lastSlash = path.find_last_of(L"\\/");
    path = path.substr(0, lastSlash) + L"\\frontend\\dist";
    return path;
}

/**
 * @brief Creates a COM VARIANT containing an IDispatch pointer.
 *
 * Used to pass the NativeBridge IDispatch to AddHostObjectToScript.
 * The VARIANT takes a reference on the dispatch pointer.
 *
 * @param pDisp The IDispatch pointer to wrap.
 * @return A VARIANT of type VT_DISPATCH.
 */
VARIANT makeDispatchVariant(IDispatch* pDisp) {
    VARIANT v;
    VariantInit(&v);
    V_VT(&v) = VT_DISPATCH;
    V_DISPATCH(&v) = pDisp;
    if (pDisp) pDisp->AddRef();
    return v;
}

} // namespace

// ── Custom QWidget for resize handling ──

/**
 * @brief QWidget subclass that handles resizing and close-to-tray behavior.
 *
 * - `resizeEvent`: Forwards size changes to the WebView2 controller so the
 *   web content fills the window.
 * - `closeEvent`: Hides the window instead of closing it. Closing would
 *   destroy the underlying HWND, detaching WebView2's compositor from its
 *   render target and causing invisible content on restore. The app is
 *   quit from the system tray menu instead.
 */
class WebView2Widget : public QWidget {
public:
    /** Callback invoked on resize with the new width and height. */
    std::function<void(int, int)> onResize;

protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        if (onResize) onResize(event->size().width(), event->size().height());
    }

    void closeEvent(QCloseEvent* event) override {
        // Hide to system tray instead of closing the native window.
        // Closing would destroy/recreate the HWND, detaching WebView2's
        // compositor from its render target → invisible content on restore.
        hide();
        event->ignore();
    }
};

// ── Constructor ──

WebView2Host::WebView2Host(WallpaperController* controller, QObject* parent)
    : QObject(parent), m_controller(controller)
{
    auto* w = new WebView2Widget();
    w->setWindowTitle("Animura");
    w->resize(kDefaultWidth, kDefaultHeight);
    w->setMinimumSize(800, 500);
    w->setStyleSheet("background-color: #0d0620;");

    // Center the window on the primary screen.
    if (auto* screen = QApplication::primaryScreen()) {
        QRect geo = screen->availableGeometry();
        w->move(
            (geo.width() - kDefaultWidth) / 2,
            (geo.height() - kDefaultHeight) / 2
        );
    }

    // Forward resize events to the WebView2 controller so the
    // web content fills the window.
    w->onResize = [this](int width, int height) {
        if (m_webViewController) {
            RECT bounds = {0, 0, width, height};
            m_webViewController->put_Bounds(bounds);
        }
    };

    QObject::connect(w, &QWidget::destroyed, this, [this]() {
        emit windowClosing();
    });

    m_window = w;
    initWebView2();
}

WebView2Host::~WebView2Host() {
    // Release COM objects in reverse order of creation.
    if (m_webViewController) {
        m_webViewController->Close();
        m_webViewController->Release();
    }
    if (m_webView) {
        m_webView->Release();
    }
    if (m_env) {
        m_env->Release();
    }
    if (m_bridge) {
        m_bridge->Release();
    }
    if (m_window) {
        m_window->close();
        delete m_window;
    }
}

// ── Public API ──

void WebView2Host::postMessageToJs(const QString& json) {
    if (!m_webView) return;
    std::wstring wstr = json.toStdWString();
    m_webView->PostWebMessageAsJson(wstr.c_str());
}

// ── WebView2 Init ──

void WebView2Host::initWebView2() {
    HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    m_window->show();

    /**
     * Asynchronously creates the WebView2 environment.
     *
     * The callback chain is:
     * 1. Environment created → create NativeBridge, start controller creation.
     * 2. Controller created → configure settings, register host objects,
     *    set up virtual host mappings, add event handlers, navigate to app.
     */
    auto envHandler = Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [this, hwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(result)) {
                qCritical() << "Failed to create WebView2 environment:" << result;
                return result;
            }
            m_env = env;
            m_env->AddRef();

            // Create the NativeBridge COM object. It is AddRef'd here
            // and also AddRef'd when passed to AddHostObjectToScript.
            m_bridge = new NativeBridge(m_controller, m_window);
            m_bridge->AddRef();

            m_bridge->setMessageCallback([this](const std::string&) {});

            return m_env->CreateCoreWebView2Controller(
                hwnd,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [this, hwnd](HRESULT cResult, ICoreWebView2Controller* controller) -> HRESULT {
                        if (FAILED(cResult)) {
                            qCritical() << "Failed to create controller:" << cResult;
                            return cResult;
                        }
                        m_webViewController = controller;
                        m_webViewController->AddRef();
                        m_webViewController->get_CoreWebView2(&m_webView);

                        if (m_webView) {
                            // ── Configure WebView2 settings ──
                            ICoreWebView2Settings* settings = nullptr;
                            m_webView->get_Settings(&settings);
                            if (settings) {
                                settings->put_IsScriptEnabled(TRUE);
                                settings->put_AreDefaultScriptDialogsEnabled(FALSE);
                                settings->put_IsWebMessageEnabled(TRUE);
                                settings->put_AreDevToolsEnabled(kDevMode);
                                settings->put_AreDefaultContextMenusEnabled(FALSE);
                            }

                            // Register native bridge (IDispatch as VARIANT).
                            // This makes the bridge available to JS as:
                            //   window.chrome.webview.hostObjects.nativeBridge
                            VARIANT v = makeDispatchVariant(
                                static_cast<IDispatch*>(m_bridge));
                            m_webView->AddHostObjectToScript(L"nativeBridge", &v);
                            VariantClear(&v);

                            // Set up virtual host mapping for modules directory
                            // so preview images can be loaded by the React UI
                            // via https://animura.modules/<folder>/<file>.
                            ICoreWebView2_3* webView3 = nullptr;
                            if (SUCCEEDED(m_webView->QueryInterface(
                                    IID_PPV_ARGS(&webView3))) && webView3) {
                                wchar_t exePath[MAX_PATH];
                                GetModuleFileNameW(nullptr, exePath, MAX_PATH);
                                std::wstring path(exePath);
                                auto lastSlash = path.find_last_of(L"\\/");
                                std::wstring modulesPath =
                                    path.substr(0, lastSlash) + L"\\modules\\";
                                webView3->SetVirtualHostNameToFolderMapping(
                                    L"animura.modules",
                                    modulesPath.c_str(),
                                    COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                                webView3->Release();
                                qDebug() << "Modules virtual host mapped:"
                                         << QString::fromStdWString(modulesPath);
                            }

                            // Web message handler — receives messages from JS.
                            // Currently no JS→C++ messages are sent this way;
                            // all communication uses the NativeBridge host object.
                            LocalEventToken msgToken;
                            m_webView->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                        LPWSTR msg = nullptr;
                                        if (SUCCEEDED(args->TryGetWebMessageAsString(&msg)) && msg) {
                                            CoTaskMemFree(msg);
                                        }
                                        return S_OK;
                                    }
                                ).Get(),
                                msgToken
                            );
                            m_webMessageToken = msgToken.value;

                            // Navigation completed handler.
                            LocalEventToken navToken;
                            m_webView->add_NavigationCompleted(
                                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                                        m_initialized = true;
                                        qDebug() << "WebView2 navigation completed";
                                        return S_OK;
                                    }
                                ).Get(),
                                navToken
                            );
                            m_navCompletedToken = navToken.value;

                            navigateToApp();
                        }

                        // Size the WebView2 to fill the window.
                        RECT bounds;
                        GetClientRect(hwnd, &bounds);
                        m_webViewController->put_Bounds(bounds);

                        return S_OK;
                    }
                ).Get()
            );
        }
    );

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr, envHandler.Get()
    );
}

void WebView2Host::navigateToApp() {
    if (!m_webView) return;

    // Build the modules folder path for virtual host mapping
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring basePath(exePath);
    auto lastSlash = basePath.find_last_of(L"\\/");
    std::wstring modulesPath =
        basePath.substr(0, lastSlash) + L"\\modules\\";
    std::wstring frontendPath = getFrontendPath() + L"\\";

    if (kDevMode) {
        // Map virtual hosts, then navigate to dev server.
        // The Vite HMR dev server provides instant reload on frontend changes.
        ICoreWebView2_3* webView3 = nullptr;
        if (SUCCEEDED(m_webView->QueryInterface(IID_PPV_ARGS(&webView3))) && webView3) {
            webView3->SetVirtualHostNameToFolderMapping(
                L"animura.modules",
                modulesPath.c_str(),
                COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
            webView3->Release();
        }
        m_webView->Navigate(kDevUrl);
        qDebug() << "WebView2 navigating to dev server:"
                 << QString::fromWCharArray(kDevUrl);
    } else {
        // Production mode: map animura.app virtual host to frontend/dist/
        // and navigate via the virtual host URL.
        ICoreWebView2_3* webView3 = nullptr;
        if (SUCCEEDED(m_webView->QueryInterface(IID_PPV_ARGS(&webView3))) && webView3) {
            webView3->SetVirtualHostNameToFolderMapping(
                kVirtualHost,
                frontendPath.c_str(),
                COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
            webView3->SetVirtualHostNameToFolderMapping(
                L"animura.modules",
                modulesPath.c_str(),
                COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
            webView3->Navigate(L"https://animura.app/index.html");
            webView3->Release();
            qDebug() << "WebView2 navigating to production build:"
                     << QString::fromStdWString(frontendPath);
        } else {
            // Fallback: navigate to file directly.
            // Virtual host mapping is preferred but requires ICoreWebView2_3.
            std::wstring indexPath = getFrontendPath() + L"\\index.html";
            m_webView->Navigate(indexPath.c_str());
            qDebug() << "WebView2 navigating to file:" << QString::fromStdWString(indexPath);
        }
    }
}
