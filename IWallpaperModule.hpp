#pragma once

#include <Windows.h>

class IWallpaperModule {
public:
    virtual ~IWallpaperModule() = default;

    virtual void run() = 0;
    virtual void stop() = 0;

    virtual HWND hwnd() const = 0;
};
