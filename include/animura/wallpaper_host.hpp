#pragma once

#include "animura/desktop_host_resolver.hpp"

namespace animura::desktop {

// ------------------------------------------------------------------
// RAII wrapper that attaches a client HWND to the desktop host.
// ------------------------------------------------------------------
class WallpaperHost {
public:
    explicit WallpaperHost(DesktopHostInfo info);
    ~WallpaperHost();

    WallpaperHost(const WallpaperHost&) = delete;
    WallpaperHost& operator=(const WallpaperHost&) = delete;
    WallpaperHost(WallpaperHost&& other) noexcept;
    WallpaperHost& operator=(WallpaperHost&& other) noexcept;

    // Attach a window to the resolved desktop host.
    // Returns true on success.
    bool Attach(HWND hwndWallpaper);

    // Detach and restore the window to its own top-level state.
    bool Detach();

    // Re-assert Z-order (call after shell restarts, resolution changes, etc.)
    bool UpdateZOrder();

    [[nodiscard]] bool IsAttached() const noexcept { return m_attached; }
    [[nodiscard]] HWND GetHost() const noexcept { return m_info.hwndHost; }
    [[nodiscard]] const DesktopHostInfo& GetInfo() const noexcept { return m_info; }

private:
    bool ApplyHostStyles(HWND hwnd);
    bool PositionAndLayer(HWND hwnd);

    DesktopHostInfo m_info;
    HWND m_hwndWallpaper{nullptr};
    bool m_attached{false};
};

} // namespace animura::desktop