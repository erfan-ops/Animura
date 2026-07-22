#pragma once

#include <string>
#include <memory>

#include <Windows.h>
#include <shobjidl.h>
#include <wrl/client.h>


namespace animura::desktop {

// ------------------------------------------------------------------
// Thin wrapper around IDesktopWallpaper COM interface with registry
// fallback. Kept separate from host resolution concerns.
// ------------------------------------------------------------------
class SystemWallpaperBridge {
public:
    SystemWallpaperBridge();
    ~SystemWallpaperBridge();

    SystemWallpaperBridge(const SystemWallpaperBridge&) = delete;
    SystemWallpaperBridge& operator=(const SystemWallpaperBridge&) = delete;

    bool SetWallpaper(std::wstring& path) const;
    [[nodiscard]] std::wstring GetWallpaper() const;

private:
    void InitializeCom();

private:
    Microsoft::WRL::ComPtr<IDesktopWallpaper> desktopWallpaperHandle;
    bool comInitizlized_{false};
};

} // namespace animura::desktop