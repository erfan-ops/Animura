#pragma once

#include <string>
#include <Windows.h>
#include "IWallpaperModule.hpp"

/**
 * @brief Function pointer type for the `createModule` factory function
 *        exported by every wallpaper module DLL.
 *
 * Signature: `extern "C" __declspec(dllexport) IWallpaperModule* createModule(const char* settingsJson)`
 *
 * @param settingsJson JSON string of module settings (from `settings.json`).
 * @return Heap-allocated IWallpaperModule*. Ownership transfers to the caller.
 *         Returns nullptr on failure.
 */
using CreateModuleFn = IWallpaperModule * (*)(const char*);

/**
 * @brief Wraps Win32 DLL loading and unloading for wallpaper modules.
 *
 * Isolates platform-specific DLL management (`LoadLibraryExW`,
 * `FreeLibrary`, `GetProcAddress`) from the business logic in
 * WallpaperController. Owns the HMODULE handle and automatically
 * frees it on destruction (RAII).
 *
 * ## Load Sequence
 * 1. `load(path)` → `unload()` any previously loaded DLL.
 * 2. Resolve absolute path.
 * 3. `LoadLibraryExW` with search flags that include the DLL's own
 *    directory (so bundled dependencies like `glfw3.dll` are found).
 * 4. `GetProcAddress(m_lib, "createModule")` → store function pointer.
 *
 * ## Critical Rule
 * The DLL must NOT be unloaded while any code inside it is executing.
 * The correct sequence is:
 * 1. `m_module->stop()` — signal render loop to exit.
 * 2. `m_worker.join()` — wait for `run()` to return.
 * 3. `m_module.reset()` — destroy the module object.
 * 4. `m_library.unload()` — NOW safe to free the DLL.
 *
 * @see WallpaperController for the correct lifecycle ordering.
 * @see IWallpaperModule for the module interface.
 */
class ModuleLibrary {
public:
    /**
     * @brief Destructor. Calls `unload()` to free the DLL if loaded.
     */
    ~ModuleLibrary();

    /**
     * @brief Loads a module DLL and resolves its `createModule` export.
     *
     * Calls `unload()` first if a DLL is currently loaded. Uses
     * `LoadLibraryExW` with flags that search the DLL's own directory,
     * system directories, and the application directory.
     *
     * On failure, `outError` is populated with a human-readable error
     * message including the Win32 error code and description (obtained
     * via `FormatMessageW`).
     *
     * @param dllPath  Path to the module DLL file.
     * @param outError Output parameter for error description on failure.
     * @return true if the DLL was loaded and `createModule` was resolved.
     */
    bool load(const std::wstring& dllPath, std::string& outError);

    /**
     * @brief Unloads the currently loaded DLL via `FreeLibrary`.
     *
     * Resets `m_lib` and `m_createFn` to null. Safe to call when no DLL
     * is loaded (no-op).
     *
     * @warning Must be called AFTER the module object has been destroyed,
     *          never while code inside the DLL is executing.
     */
    void unload();

    /**
     * @brief Returns the resolved `createModule` factory function pointer.
     * @return The function pointer, or nullptr if no DLL is loaded.
     */
    CreateModuleFn createFn() const { return m_createFn; }

    /**
     * @brief Returns whether a DLL is currently loaded.
     * @return true if `load()` succeeded and `unload()` has not been called.
     */
    bool isLoaded() const { return m_lib != nullptr; }

private:
    /**
     * Handle to the loaded DLL. nullptr when no DLL is loaded.
     * Obtained from `LoadLibraryExW`, freed by `FreeLibrary`.
     */
    HMODULE m_lib{ nullptr };

    /**
     * Cached pointer to the `createModule` export.
     * Resolved by `GetProcAddress` during `load()`. nullptr when no DLL is loaded.
     */
    CreateModuleFn m_createFn{ nullptr };
};
