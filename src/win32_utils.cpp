#include "animura/win32_utils.hpp"

#include <combaseapi.h>

namespace animura::win32 {

ScopedCoInit::ScopedCoInit(DWORD apartment) : m_hr(CoInitializeEx(nullptr, apartment)) {}
ScopedCoInit::~ScopedCoInit() { if (SUCCEEDED(m_hr)) CoUninitialize(); }

LONG_PTR GetWindowStyle(HWND hwnd) { return GetWindowLongPtrW(hwnd, GWL_STYLE); }
LONG_PTR GetWindowExStyle(HWND hwnd) { return GetWindowLongPtrW(hwnd, GWL_EXSTYLE); }

bool HasWindowStyle(HWND hwnd, LONG_PTR style) {
    return hwnd && (GetWindowStyle(hwnd) & style) != 0;
}

bool HasExtendedStyle(HWND hwnd, LONG_PTR style) {
    return hwnd && (GetWindowExStyle(hwnd) & style) != 0;
}

void SanitizeWindowStyles(HWND hwnd, bool makeChild) {
    LONG_PTR style = GetWindowStyle(hwnd);
    LONG_PTR exStyle = GetWindowExStyle(hwnd);

    // Remove all window chrome so the wallpaper looks seamless.
    constexpr LONG_PTR kStyleMask = WS_CAPTION | WS_THICKFRAME | WS_SYSMENU |
                                    WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_BORDER |
                                    WS_DLGFRAME;
    style &= ~kStyleMask;

    if (makeChild) {
        style &= ~WS_POPUP;
        style |= WS_CHILD;
    } else {
        style &= ~WS_CHILD;
        style |= WS_POPUP;
    }

    // Prevent the window from stealing focus or appearing on the taskbar.
    exStyle |= WS_EX_NOACTIVATE;
    exStyle &= ~WS_EX_APPWINDOW;
    exStyle |= WS_EX_TOOLWINDOW; // Keep us off the taskbar

    SetWindowLongPtrW(hwnd, GWL_STYLE, style);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);
}

VirtualScreenGeometry GetVirtualScreenGeometry() {
    return {
        GetSystemMetrics(SM_XVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN),
        GetSystemMetrics(SM_CXVIRTUALSCREEN),
        GetSystemMetrics(SM_CYVIRTUALSCREEN)
    };
}

std::wstring GetLastErrorString() {
    DWORD err = GetLastError();
    if (err == 0) return L"";
    LPWSTR buf = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
    std::wstring msg(buf ? buf : L"");
    if (buf) LocalFree(buf);
    return msg;
}

} // namespace animura::win32