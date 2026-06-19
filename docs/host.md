# Host Application

The Animura host is a Qt 6 C++ application that manages wallpaper module discovery, loading, lifecycle, and provides a QML settings UI.

## Entry Point: `src/main.cpp`

```
main()
  ├─ SetDefaultDllDirectories()          // DLL search path security
  ├─ QApplication                        // Qt app instance
  ├─ QLockFile ("Animura.lock")          // Single-instance enforcement
  │   └─ If locked: notify running instance via QLocalSocket, exit
  ├─ QQmlApplicationEngine               // QML engine
  ├─ WallpaperController (ctor)          // Scans /modules directory
  ├─ ColorDialogHelper                   // Exposes QColorDialog to QML
  ├─ Context properties:                 // Exposed to QML
  │   ├─ "backend" → WallpaperController
  │   ├─ "ColorDialogHelper" → color picker
  │   └─ "WindowsAccent" → DWM accent color
  ├─ QLocalServer                        // Receives "show" from other instances
  ├─ QSystemTrayIcon                     // Tray icon with Open/Quit menu
  └─ app.exec()                          // Qt event loop
```

### Single-Instance Pattern
- Tries to acquire `Animura.lock` in `%TEMP%`.
- If locked, sends "show" to existing instance via `QLocalSocket`, then exits.
- Existing instance calls `showMainWindow()` on the root QML window.

### Tray Icon
- "Open Animura" → shows and raises the window.
- "Quit" → calls `QCoreApplication::quit()`.
- `onClosing` in QML hides the window instead of closing — the app lives in the tray.

---

## Core Class: `WallpaperController`

**File:** `src/WallpaperController.cpp`, `include/animura/WallpaperController.hpp`

The central orchestrator. Lives on the Qt main thread. Exposed to QML as `backend`.

### Members

| Member | Type | Purpose |
|---|---|---|
| `m_catalog` | `ModuleCatalog` | Discovered module list from `/modules` |
| `m_library` | `ModuleLibrary` | Currently loaded DLL handle |
| `m_module` | `std::unique_ptr<IWallpaperModule>` | Active module instance |
| `m_worker` | `std::thread` | Worker thread running the module |
| `m_running` | `std::atomic_bool` | Whether a module is currently active |
| `m_currentModuleId` | `int` | Index of currently loaded module |
| `m_originalWallpaper` | `std::wstring` | Saved wallpaper path for restore |

### Public API (Q_INVOKABLE — callable from QML)

#### `getModulesList() → QVariantList`
Returns list of `{name, version, previewPath, id}` for each discovered module.

#### `startWallpaper(int moduleIndex)`
Full start sequence:
1. Validate index
2. Validate files exist (dll, schema, settings)
3. Validate settings against schema
4. `ensureLibraryLoaded()` — if switching modules, stops current and loads new DLL
5. If already running, calls `stopWallpaper()`
6. `startWorker()` — spawns worker thread

#### `stopWallpaper()`
Full stop sequence:
1. Guard: if `!m_running`, return
2. If `m_module` exists:
   - Detach from desktop (`DetachWindowFromDesktop`)
   - Hide window style (`~WS_VISIBLE`)
   - Call `m_module->stop()` (sets atomic flag)
3. Join worker thread (or detach if called from worker thread)
4. Set `m_running = false`
5. `m_module.reset()` — **destroys module** (see critical note below)
6. `restoreWallpaper()` — restores original Windows wallpaper

> **⚠️ CRITICAL:** `m_module.reset()` at step 5 runs on the **main thread**, but the module was created on the **worker thread**. This causes `glfwTerminate()` and GL object deletion to run on the wrong thread. See `/docs/runtime.md` for the fix.

#### `loadSettingsUI(int moduleIndex) → QJsonObject`
Returns `{schema, settings}` for the settings panel QML.

#### `applySettings(int moduleIndex, QJsonObject)`
Writes settings JSON to disk using `QSaveFile` (atomic write).

### Private Helpers

| Method | Purpose |
|---|---|
| `validateIndex(int)` | Bounds check on module list |
| `validateModuleFiles(ModuleInfo)` | Check dll, schema, settings files exist |
| `validateModuleSettings(ModuleInfo)` | Validate settings JSON against schema |
| `ensureLibraryLoaded(ModuleInfo, int)` | Load DLL if needed, stop current if running |
| `startWorker(ModuleInfo)` | Spawn worker thread, create and run module |
| `restoreWallpaper()` | Set Windows wallpaper back to saved path |

---

## ModuleCatalog

**File:** `src/ModuleCatalog.cpp`, `include/animura/ModuleCatalog.hpp`

Scans `./modules/` for subdirectories containing `module.json` files. Each valid module directory produces one `ModuleInfo` struct.

### ModuleInfo

```cpp
struct ModuleInfo {
    std::filesystem::path basePath;
    std::string name;           // from module.json
    std::string version;        // from module.json
    std::filesystem::path entryDll;     // relative: "module.dll"
    std::filesystem::path schemaFile;   // relative: "schema.json"
    std::filesystem::path settingsFile; // relative: "settings.json"
    std::filesystem::path previewFile;  // relative: "preview.jpg"
};
```

### Required JSON fields in `module.json`
- `name` — display name
- `version` — semantic version string
- `entry` — DLL filename (e.g. `"module.dll"`)
- `schema` — schema filename (e.g. `"schema.json"`)
- `settings` — settings filename (e.g. `"settings.json"`)
- `preview` — preview image filename (e.g. `"preview.jpg"`)

---

## ModuleLibrary

**File:** `src/ModuleLibrary.cpp`, `include/animura/ModuleLibrary.hpp`

Wraps Win32 DLL loading.

```cpp
using CreateModuleFn = IWallpaperModule* (*)(const char*);

class ModuleLibrary {
    HMODULE m_lib{nullptr};
    CreateModuleFn m_createFn{nullptr};
public:
    bool load(const std::wstring& dllPath, std::string& outError);
    void unload();
    CreateModuleFn createFn() const;
    bool isLoaded() const;
};
```

### Load sequence
1. Resolve absolute path
2. `LoadLibraryExW` with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_APPLICATION_DIR`
3. `GetProcAddress(m_lib, "createModule")` — resolve factory function
4. Store function pointer as `m_createFn`

### Unload
- `FreeLibrary(m_lib)` — decrements DLL reference count, may unload
- Sets `m_lib = nullptr`, `m_createFn = nullptr`

### Ownership note
The `ModuleLibrary` owns the `HMODULE` but does NOT own the module object. The `IWallpaperModule*` returned by `createFn()` is owned by `WallpaperController::m_module` (`unique_ptr`).

---

## SettingsSchemaValidator

**File:** `src/SettingsSchemaValidator.cpp`, `include/animura/SettingsSchemaValidator.hpp`

Recursive JSON schema validator. Walks the schema tree and checks each setting value.

### Supported constraints
| Constraint | Description |
|---|---|
| `type: "int"` | Value must be integer |
| `type: "float"` | Value must be number |
| `type: "bool"` | Value must be boolean |
| `type: "string"` | Value must be string |
| `min` / `max` | Numeric range validation |
| `options: [...]` | Value must be one of the listed options |
| Nested objects | Recurse into sub-objects |

Unknown keys in settings (not defined in schema) are ignored. Missing keys in settings (defined in schema) are reported as errors.

---

## ColorDialogHelper

**File:** `include/animura/ColorDialogHelper.hpp`

Minimal QML bridge to `QColorDialog::getColor()`. Exposed as `ColorDialogHelper` context property.

---

## JsonUtils

**File:** `src/JsonUtils.cpp`, `include/animura/JsonUtils.hpp`

Single function: `readJsonFile(path, json& out, string* error)` — reads and parses a JSON file with optional error reporting.
