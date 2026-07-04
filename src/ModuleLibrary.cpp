/**
 * @file ModuleLibrary.cpp
 * @brief Implementation of Win32 DLL loading for wallpaper modules.
 *
 * Wraps `LoadLibraryExW`, `FreeLibrary`, and `GetProcAddress` in a simple
 * RAII class. Isolates platform-specific DLL management from the
 * WallpaperController business logic.
 *
 * ## Load Flags
 * `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` — Searches the DLL's own directory
 * first. This is how `glfw3.dll` (bundled alongside `module.dll`) is found.
 *
 * `LOAD_LIBRARY_SEARCH_DEFAULT_DIRS` — Searches system directories.
 *
 * `LOAD_LIBRARY_SEARCH_APPLICATION_DIR` — Searches the directory containing
 * the host executable.
 *
 * @see WallpaperController for the correct lifecycle ordering.
 */

#include "ModuleLibrary.hpp"
#include <sstream>
#include <iostream>
#include <filesystem>

ModuleLibrary::~ModuleLibrary() {
    unload();
}

bool ModuleLibrary::load(const std::wstring& dllPath, std::string& outError) {
    // Always unload any previously loaded DLL first.
    unload();

    std::wstring fullPath = std::filesystem::absolute(dllPath).wstring();
    m_lib = LoadLibraryExW(
        fullPath.c_str(),
        nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_APPLICATION_DIR
    );
    if (!m_lib) {
        DWORD err = GetLastError();

        // Format the Win32 error code into a human-readable message.
        LPWSTR buffer = nullptr;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            err,
            0,
            (LPWSTR)&buffer,
            0,
            nullptr
        );

        std::wstring message = buffer ? buffer : L"Unknown error";
        LocalFree(buffer);

        outError = "LoadLibraryEx failed. Error: " +
            std::to_string(err) +
            " - " +
            std::string(message.begin(), message.end());

        return false;
    }

    // Resolve the `createModule` export — the factory function every
    // wallpaper module DLL must export.
    m_createFn = reinterpret_cast<CreateModuleFn>(GetProcAddress(m_lib, "createModule"));
    if (!m_createFn) {
        outError = "Function createModule not found";
        unload();
        return false;
    }

    return true;
}

void ModuleLibrary::unload() {
    if (m_lib) {
        FreeLibrary(m_lib);
        m_lib = nullptr;
        m_createFn = nullptr;
    }
}
