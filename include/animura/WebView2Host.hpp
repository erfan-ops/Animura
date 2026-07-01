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
 * Hosts a Microsoft Edge WebView2 control inside a Qt QWidget window.
 *
 * Responsibilities:
 * - Create a top-level QWidget window
 * - Initialize the WebView2 environment
 * - Create and configure the WebView2 controller
 * - Register the NativeBridge host object for JS ↔ C++ communication
 * - Handle navigation (dev server or production build)
 * - Forward messages from C++ to JavaScript
 */
class WebView2Host : public QObject {
    Q_OBJECT

public:
    explicit WebView2Host(WallpaperController* controller, QObject* parent = nullptr);
    ~WebView2Host() override;

    QWidget* widget() const { return m_window; }

    /** Send a JSON message to JavaScript */
    void postMessageToJs(const QString& json);

signals:
    void windowClosing();

private:
    void initWebView2();
    void navigateToApp();

    QWidget* m_window{ nullptr };
    WallpaperController* m_controller{ nullptr };
    NativeBridge* m_bridge{ nullptr };

    // WebView2 COM pointers (raw — released in destructor)
    ICoreWebView2Environment* m_env{ nullptr };
    ICoreWebView2Controller* m_webViewController{ nullptr };
    ICoreWebView2* m_webView{ nullptr };

    bool m_initialized{ false };

    // Event tokens stored as 64-bit values (EventRegistrationToken is a long long)
    long long m_navCompletedToken{ 0 };
    long long m_webMessageToken{ 0 };
};
