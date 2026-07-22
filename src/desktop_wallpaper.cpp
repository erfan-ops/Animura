#include <desktop_wallpaper.hpp>


namespace animura::desktop {


SystemWallpaperBridge::SystemWallpaperBridge() {
    InitializeCom();
}

SystemWallpaperBridge::~SystemWallpaperBridge() {
    if (comInitizlized_) {
        desktopWallpaperHandle.Reset();
        CoUninitialize();
    }
}

void SystemWallpaperBridge::InitializeCom() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    HRESULT hr = CoCreateInstance(
        CLSID_DesktopWallpaper,
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&desktopWallpaperHandle)
    );

    comInitizlized_ = SUCCEEDED(hr);
}

[[nodiscard]] std::wstring SystemWallpaperBridge::GetWallpaper() const {
    if (!comInitizlized_) {
        // Get Wallpaper Path from the IDesktopWallpaper COM interface
        LPWSTR wallpaperPath = nullptr;
        desktopWallpaperHandle->GetWallpaper(nullptr, &wallpaperPath);

        std::wstring results = wallpaperPath;

        CoTaskMemFree(wallpaperPath);
        
        return results;
    }
    else {
        // Get Wallpaper Path from the registery
        wchar_t buf[MAX_PATH] = {};
        if (SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, buf, 0)) {
            return std::wstring(buf);
        }
        return std::wstring();
    }
}

bool SystemWallpaperBridge::SetWallpaper(std::wstring& path) const {
    if (!comInitizlized_) {
        // Set Wallpaper Path using the IDesktopWallpaper COM interface
        HRESULT hr = desktopWallpaperHandle->SetWallpaper(nullptr, path.c_str());
        return SUCCEEDED(hr);
    }
    else {
        // Set Wallpaper Path using the registery
        return SystemParametersInfo(
            SPI_SETDESKWALLPAPER,
            0,
            path.data(),
            SPIF_SENDCHANGE
        ) == TRUE;
    }
}

}