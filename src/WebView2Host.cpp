#include "WebView2Host.hpp"
#include "NativeBridge.hpp"
#include "WallpaperController.hpp"

#include <QWidget>
#include <QApplication>
#include <QScreen>
#include <QDebug>
#include <QResizeEvent>

#include <WebView2.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <windows.h>
#include <ShObjIdl.h>

using Microsoft::WRL::Callback;

// EventRegistrationToken-compatible local struct (avoids winrt/EventToken.h include)
struct LocalEventToken {
    long long value;
    operator EventRegistrationToken*() {
        return reinterpret_cast<EventRegistrationToken*>(this);
    }
};

namespace {

constexpr bool kDevMode = false;
constexpr const wchar_t* kDevUrl = L"http://localhost:5173";
constexpr const wchar_t* kVirtualHost = L"animura.app";
constexpr int kDefaultWidth = 1280;
constexpr int kDefaultHeight = 720;

std::wstring getFrontendPath() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring path(exePath);
    auto lastSlash = path.find_last_of(L"\\/");
    path = path.substr(0, lastSlash) + L"\\frontend\\dist";
    return path;
}

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
class WebView2Widget : public QWidget {
public:
    std::function<void(int, int)> onResize;

protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        if (onResize) onResize(event->size().width(), event->size().height());
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

    if (auto* screen = QApplication::primaryScreen()) {
        QRect geo = screen->availableGeometry();
        w->move(
            (geo.width() - kDefaultWidth) / 2,
            (geo.height() - kDefaultHeight) / 2
        );
    }

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

    auto envHandler = Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [this, hwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(result)) {
                qCritical() << "Failed to create WebView2 environment:" << result;
                return result;
            }
            m_env = env;
            m_env->AddRef();

            m_bridge = new NativeBridge(m_controller);
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
                            ICoreWebView2Settings* settings = nullptr;
                            m_webView->get_Settings(&settings);
                            if (settings) {
                                settings->put_IsScriptEnabled(TRUE);
                                settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                                settings->put_IsWebMessageEnabled(TRUE);
                                settings->put_AreDevToolsEnabled(kDevMode);
                                settings->put_AreDefaultContextMenusEnabled(FALSE);
                            }

                            // Register native bridge (IDispatch as VARIANT)
                            VARIANT v = makeDispatchVariant(
                                static_cast<IDispatch*>(m_bridge));
                            m_webView->AddHostObjectToScript(L"nativeBridge", &v);
                            VariantClear(&v);

                            // Set up virtual host mapping for modules directory
                            // so preview images can be loaded by the React UI.
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

                            // Web message handler
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

                            // Navigation completed
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
        // Map virtual hosts, then navigate to dev server
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
        // Try to get ICoreWebView2_3 for virtual host mapping
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
            // Fallback: navigate to file directly
            std::wstring indexPath = getFrontendPath() + L"\\index.html";
            m_webView->Navigate(indexPath.c_str());
            qDebug() << "WebView2 navigating to file:" << QString::fromStdWString(indexPath);
        }
    }
}
