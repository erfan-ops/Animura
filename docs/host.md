# Host Application

The Animura host is a Qt 6 C++ application that manages wallpaper module discovery, loading, lifecycle, and hosts a React frontend inside Microsoft Edge WebView2.

## Entry Point: `src/main.cpp`

```
main()
  ├─ SetDefaultDllDirectories()          // DLL search path security
  ├─ QApplication                        // Qt app instance
  ├─ QLockFile ("Animura.lock")          // Single-instance enforcement
  │   └─ If locked: notify running instance via QLocalSocket, exit
  ├─ WallpaperController (ctor)          // Scans /modules directory
  ├─ WebView2Host                        // Creates QWidget + WebView2 control
  │   ├─ NativeBridge registered         // COM IDispatch → JS bridge
  │   ├─ Virtual hosts mapped:
  │   │   ├─ animura.app → frontend/dist/
  │   │   └─ animura.modules → modules/
  │   └─ Navigate to production build or dev server
  ├─ QObject::connect: runningModuleChanged → PostWebMessageAsJson
  ├─ QLocalServer                        // Receives "show" from other instances
  ├─ QSystemTrayIcon                     // Tray icon with Open/Quit menu
  └─ app.exec()                          // Qt event loop
```

### Single-Instance Pattern
- Tries to acquire `Animura.lock` in `%TEMP%`.
- If locked, sends "show" to existing instance via `QLocalSocket`, then exits.
- Existing instance shows and raises its window.

### Tray Icon
- "Open Animura" → shows and raises the window.
- "Quit" → calls `QCoreApplication::quit()`.
- The app lives in the tray — closing the window hides it, does not quit.

---

## Core Class: `WallpaperController`

**File:** `src/WallpaperController.cpp`, `include/animura/WallpaperController.hpp`

The central orchestrator. Lives on the Qt main thread. Methods are called from React via the NativeBridge (COM) with thread marshaling.

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

### Public API (Q_INVOKABLE — called via NativeBridge)

#### `getModulesList() → QVariantList`
Returns list of `{name, version, previewPath, id}` for each discovered module. `previewPath` is a virtual-host URL (`https://animura.modules/<name>/<file>`) resolved by WebView2 to the modules folder.

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
   - Hide window style
   - Call `m_module->stop()` (sets atomic flag)
3. Join worker thread (or detach if called from worker thread)
4. Set `m_running = false`
5. `m_module.reset()` — **destroys module** (see critical note below)
6. `restoreWallpaper()` — restores original Windows wallpaper

> **⚠️ CRITICAL:** `m_module.reset()` runs on the **main thread**, but the module was created on the **worker thread**. This causes `glfwTerminate()` and GL object deletion to run on the wrong thread. See `/docs/runtime.md` for the fix.

#### `loadSettingsUI(int moduleIndex) → QJsonObject`
Returns `{schema, settings}` for the settings panel React component.

#### `applySettings(int moduleIndex, QJsonObject)`
Writes settings JSON to disk using `QSaveFile` (atomic write).

### Signals

| Signal | Emitted when | Connected to |
|---|---|---|
| `runningModuleChanged()` | Module starts or stops | WebView2 → `PostWebMessageAsJson` → React updates UI |

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

## WebView2Host

**File:** `src/WebView2Host.cpp`, `include/animura/WebView2Host.hpp`

Creates and manages a Microsoft Edge WebView2 control embedded in a Qt `QWidget`.

### Responsibilities
- Create a top-level `QWidget` window (purple background, centered on screen)
- Initialize the WebView2 environment and controller
- Configure settings (scripts enabled, context menus disabled, dev tools gated by `kDevMode`)
- Register `NativeBridge` as a COM host object → `window.chrome.webview.hostObjects.nativeBridge`
- Set up virtual host mappings:
  - `animura.app` → `frontend/dist/` (React app)
  - `animura.modules` → `modules/` (wallpaper preview images)
- Handle navigation: dev server (`http://localhost:5173`) or production (`https://animura.app/index.html`)
- Forward C++ signals to JS via `PostWebMessageAsJson`

### Key Methods
| Method | Purpose |
|---|---|
| `widget()` | Returns the QWidget for tray/single-instance integration |
| `postMessageToJs(json)` | Sends a JSON message to the React app |

---

## NativeBridge

**File:** `src/NativeBridge.cpp`, `include/animura/NativeBridge.hpp`

COM `IDispatch` implementation registered with WebView2 via `AddHostObjectToScript`. Provides the typed bridge between the React frontend and the C++ backend.

### Thread Safety
- WebView2 calls `Invoke()` on arbitrary threads
- All methods marshal to the Qt main thread via `QMetaObject::invokeMethod`
- `Qt::BlockingQueuedConnection` for cross-thread calls
- `Qt::DirectConnection` when already on the main thread

### Methods (DISPID mapping)

| DISPID | Method | Returns |
|---|---|---|
| 1 | `GetModulesList` | BSTR (JSON array) |
| 2 | `GetRunningModuleId` | BSTR (integer as string — parse with `Number()`) |
| 3 | `StartWallpaper(idx)` | void |
| 4 | `StopWallpaper` | void |
| 5 | `LoadSettingsUI(idx)` | BSTR (JSON `{schema, settings}`) |
| 6 | `ApplySettings(idx, json)` | void |
| 7 | `GetAccentColor` | BSTR (hex color) |

**Important:** All return values are `BSTR` (COM strings). The JS bridge wrapper in `frontend/src/bridge/native.ts` handles type coercion.

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
    std::filesystem::path previewFile;  // relative: "preview.png"

    // previewPathQt() returns https://animura.modules/<folder>/<file>
    // for WebView2 virtual-host resolution.
};
```

### Required JSON fields in `module.json`
- `name` — display name
- `version` — semantic version string
- `entry` — DLL filename (e.g. `"module.dll"`)
- `schema` — schema filename (e.g. `"schema.json"`)
- `settings` — settings filename (e.g. `"settings.json"`)
- `preview` — preview image filename (e.g. `"preview.png"`)

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

---

## SettingsSchemaValidator

**File:** `src/SettingsSchemaValidator.cpp`, `include/animura/SettingsSchemaValidator.hpp`

Recursive JSON schema validator. Checks type constraints, min/max ranges, allowed options, and required keys. Validation errors block module start.

---

## JsonUtils

**File:** `src/JsonUtils.cpp`, `include/animura/JsonUtils.hpp`

Single function: `readJsonFile(path, json& out, string* error)` — reads and parses a JSON file with optional error reporting.
