#pragma once

#include <string>
#include <Windows.h>
#include "IWallpaperModule.hpp"

using CreateModuleFn = IWallpaperModule * (*)(const char*);

class ModuleLibrary {
public:
    ~ModuleLibrary();

    bool load(const std::wstring& dllPath, std::string& outError);
    void unload();

    CreateModuleFn createFn() const { return m_createFn; }
    bool isLoaded() const { return m_lib != nullptr; }

private:
    HMODULE m_lib{ nullptr };
    CreateModuleFn m_createFn{ nullptr };
};
