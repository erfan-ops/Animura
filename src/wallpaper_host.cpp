#include "animura/wallpaper_host.hpp"
#include "animura/win32_utils.hpp"

#include <QDebug>
#include <iostream>

namespace animura::desktop {

WallpaperHost::WallpaperHost(DesktopHostInfo info) : m_info(info) {}

WallpaperHost::~WallpaperHost() {
    if (m_attached && m_hwndWallpaper) {
        Detach();
    }
}

WallpaperHost::WallpaperHost(WallpaperHost&& other) noexcept
    : m_info(other.m_info),
      m_hwndWallpaper(other.m_hwndWallpaper),
      m_attached(other.m_attached) {
    other.m_hwndWallpaper = nullptr;
    other.m_attached = false;
}

WallpaperHost& WallpaperHost::operator=(WallpaperHost&& other) noexcept {
    if (this != &other) {
        if (m_attached && m_hwndWallpaper) {
            Detach();
        }
        m_info = other.m_info;
        m_hwndWallpaper = other.m_hwndWallpaper;
        m_attached = other.m_attached;
        other.m_hwndWallpaper = nullptr;
        other.m_attached = false;
    }
    return *this;
}

bool WallpaperHost::Attach(HWND hwndWallpaper) {
    if (!IsWindow(hwndWallpaper)) return false;
    if (!m_info.IsValid()) return false;
    if (m_attached) return false;

    m_hwndWallpaper = hwndWallpaper;

    // 1. Reparent into the shell hierarchy.
    if (!SetParent(hwndWallpaper, m_info.hwndHost)) {
        return false;
    }

    // 2. Strip chrome and make it a proper child window.
    ApplyHostStyles(hwndWallpaper);

    // 3. Size to the virtual screen and push behind icons.
    if (!PositionAndLayer(hwndWallpaper)) {
        SetParent(hwndWallpaper, nullptr); // Rollback
        return false;
    }

    m_attached = true;
    return true;
}

bool WallpaperHost::Detach() {
    if (!m_attached || !IsWindow(m_hwndWallpaper)) {
        m_attached = false;
        m_hwndWallpaper = nullptr;
        return true;
    }

    // Restore to top-level (desktop-owned) so the renderer can clean up normally.
    SetParent(m_hwndWallpaper, nullptr);

    // Restore popup styles so it can live independently again.
    win32::SanitizeWindowStyles(m_hwndWallpaper, false);

    // Push it to the bottom of the Z-order without activating.
    SetWindowPos(m_hwndWallpaper, HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    m_attached = false;
    m_hwndWallpaper = nullptr;
    return true;
}

bool WallpaperHost::UpdateZOrder() {
    if (!m_attached || !IsWindow(m_hwndWallpaper)) return false;
    return PositionAndLayer(m_hwndWallpaper);
}

bool WallpaperHost::ApplyHostStyles(HWND hwnd) {
    win32::SanitizeWindowStyles(hwnd, true);

    // Force style update without triggering activation.
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    return true;
}

bool WallpaperHost::PositionAndLayer(HWND hwnd) {
    const auto geo = win32::GetVirtualScreenGeometry();

    // In all layouts we want the wallpaper at the bottom of the host's
    // child Z-order so that DefView (icons) renders above us.
    BOOL ok = SetWindowPos(hwnd, HWND_BOTTOM,
                           geo.x, geo.y, geo.width, geo.height,
                           SWP_NOACTIVATE | SWP_SHOWWINDOW);

    if (!ok) return false;

    // Modern Raised layout special case:
    // Progman children: [DefView (top)] [our window] [WorkerW (bottom)]
    // We must sandwich ourselves between DefView and the system WorkerW.
    if (m_info.layout == DesktopLayout::ModernRaised && m_info.hwndWorkerW) {
        // First put our window at the absolute bottom of Progman children.
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        // Then push the system WorkerW even lower, placing us above it.
        SetWindowPos(m_info.hwndWorkerW, HWND_BOTTOM, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    return true;
}

} // namespace animura::desktop