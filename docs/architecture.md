# Architecture Overview

## System Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                      Qt Main Thread                              │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  WebView2Host (QWidget + ICoreWebView2Controller)          │  │
│  │  ┌──────────────────────────────────────────────────────┐  │  │
│  │  │  React Application (TypeScript + Vite)               │  │  │
│  │  │  App.tsx → ModuleGrid → ModuleCard                   │  │  │
│  │  │         → SettingsPanel → SettingsControl (recursive)│  │  │
│  │  │         → Header, Notification                       │  │  │
│  │  └──────────────┬───────────────────────────────────────┘  │  │
│  │                 │ COM IDispatch                             │  │
│  │  ┌──────────────▼───────────────────────────────────────┐  │  │
│  │  │  NativeBridge (IDispatch → Qt main thread marshal)   │  │  │
│  │  └──────────────┬───────────────────────────────────────┘  │  │
│  └─────────────────│──────────────────────────────────────────┘  │
│                    │                                              │
│  ┌─────────────────▼──────────────────────────────────────────┐  │
│  │              WallpaperController                            │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌─────────────────┐   │  │
│  │  │ModuleCatalog│  │ModuleLibrary │  │m_module         │   │  │
│  │  │(discovery)  │  │(LoadLibrary) │  │(unique_ptr)     │   │  │
│  │  └─────────────┘  └──────────────┘  └────────┬────────┘   │  │
│  │  ┌───────────────────────────────────────────▼──────────┐  │  │
│  │  │              m_worker (std::thread)                  │  │  │
│  │  │  ┌────────────────────────────────────────────────┐  │  │  │
│  │  │  │        IWallpaperModule (DLL)                  │  │  │  │
│  │  │  │  Application::run() → mainLoop()              │  │  │  │
│  │  │  │  (OpenGL / GLFW render loop)                  │  │  │  │
│  │  │  └────────────────────────────────────────────────┘  │  │  │
│  │  └──────────────────────────────────────────────────────┘  │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                   │
│  QSystemTrayIcon  QLockFile  QLocalServer                         │
└───────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌──────────────────────────────────────────────────────────────────┐
│                   WorkerW Desktop Layer                           │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │            Module's GLFW Window                             │  │
│  │            (Attached via AttachWindowToDesktop)             │  │
│  │            Renders live wallpaper content                   │  │
│  └────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
```

## Subsystems

### 1. Host Application (`src/`, `include/animura/`)
The Qt-based executable. Manages module discovery, loading, lifecycle, and hosts the React UI inside WebView2.

| Component | Responsibility | Key Files |
|---|---|---|
| `main.cpp` | Entry point, single-instance lock, tray icon, creates WebView2Host | `src/main.cpp` |
| `WallpaperController` | Central orchestrator: load/start/stop modules, manage worker thread | `src/WallpaperController.cpp`, `.hpp` |
| `WebView2Host` | Creates a QWidget window, initializes WebView2, registers NativeBridge, navigates to frontend | `src/WebView2Host.cpp`, `.hpp` |
| `NativeBridge` | COM `IDispatch` object exposed to JS via `AddHostObjectToScript`. Marshals all calls to Qt main thread | `src/NativeBridge.cpp`, `.hpp` |
| `ModuleCatalog` | Scans `./modules/` directory, parses `module.json` metadata | `src/ModuleCatalog.cpp`, `.hpp` |
| `ModuleLibrary` | Wraps `LoadLibraryEx`/`FreeLibrary`, resolves `createModule` entry point | `src/ModuleLibrary.cpp`, `.hpp` |
| `SettingsSchemaValidator` | Validates module settings JSON against its schema before loading | `src/SettingsSchemaValidator.cpp`, `.hpp` |
| `JsonUtils` | Thin helper for reading JSON files with error reporting | `src/JsonUtils.cpp`, `.hpp` |

### 2. Frontend (`frontend/`)
React 18 + TypeScript application built with Vite 5, rendered inside WebView2.

| File | Purpose |
|---|---|
| `src/App.tsx` | Root layout — wires hooks to components |
| `src/components/ModuleGrid.tsx` | Responsive grid of ModuleCards |
| `src/components/ModuleCard.tsx` | Neumorphic preview card with hover animation |
| `src/components/SettingsPanel.tsx` | Slide-in drawer for module settings |
| `src/components/SettingsControl.tsx` | Recursive form generator from JSON schema |
| `src/components/ColorPicker.tsx` | Full color picker with HSV area, hue/alpha sliders, presets, eye dropper |
| `src/components/Header.tsx` | App header with Stop button |
| `src/components/Notification.tsx` | Toast notification system |
| `src/bridge/native.ts` | Typed wrapper around the COM NativeBridge |
| `src/hooks/useBackend.ts` | React hooks: `useModules`, `useRunningModuleId`, `useSettings`, `useStartStop` |
| `src/theme/tokens.css` | CSS custom properties (design tokens) |

### 3. Module Interface (`include/animura/IWallpaperModule.hpp`)
The ABI contract between host and module DLLs. Four virtual methods:
- `~IWallpaperModule()` — virtual destructor (required for DLL boundary)
- `run()` — blocking call, starts render loop
- `stop()` — signals render loop to exit (sets atomic flag)
- `hwnd()` — returns Win32 window handle for desktop attachment

### 4. Wallpaper Modules (`modules/`)
DLL-based plugins implementing `IWallpaperModule`. Each module ships:
- `module.dll` — the compiled plugin
- `module.json` — metadata (name, version, entry point)
- `schema.json` — JSON Schema defining configurable settings
- `settings.json` — current setting values (user-editable)
- `preview.png`/`.jpg` — preview thumbnail
- `glfw3.dll` — GLFW runtime (bundled per module)

Available modules: black-hole, delaunay-flow, eclipse-frame, fireflies, hypercube-harmony, infinite-mirror, shahr-flow, star-simulator.

### 5. Desktop Integration (`include/wallpaper-host/`, `lib/`)
Static library providing Win32 wallpaper manipulation:
- `AttachWindowToDesktop(HWND)` — parents module window to WorkerW behind desktop icons
- `DetachWindowFromDesktop(HWND)` — restores window to top-level
- `GetCurrentWallpaperPath()` / `SetWallpaper()` — save/restore original wallpaper

## Communication Flow

```
┌──────────┐  COM IDispatch   ┌──────────────┐  QMetaObject    ┌────────────────────┐
│  React   │ ────────────────►│ NativeBridge  │ ──────────────►│ WallpaperController │
│   (JS)   │                  │ (IDispatch)   │  invokeMethod  │     (QObject)       │
│          │◄─────────────────│               │◄───────────────│                     │
└──────────┘  BSTR return     └──────────────┘  Direct/BQ      └────────────────────┘
     │                              ▲              conn               │
     │  PostWebMessageAsJson        │                                 │
     │◄─────────────────────────────┘                                 │
     │  {type, moduleId}            emit runningModuleChanged()       │
     │                              signal → QObject::connect         │
```

### JS → C++ (Host Objects)
React calls C++ through the WebView2 host object proxy:
```typescript
const bridge = window.chrome.webview.hostObjects.nativeBridge;
const json = await bridge.GetModulesList();        // → BSTR JSON
const id = Number(await bridge.GetRunningModuleId()); // → BSTR, parse to number
await bridge.StartWallpaper(idx);
await bridge.StopWallpaper();
```

### C++ → JS (Web Messages)
C++ pushes events via `PostWebMessageAsJson`:
```json
{"type":"runningModuleChanged","moduleId":0}
```

### DISPID Method Map

| DISPID | Method | Returns |
|---|---|---|
| 1 | `GetModulesList` | BSTR (JSON array of ModuleInfo) |
| 2 | `GetRunningModuleId` | BSTR (number as string) |
| 3 | `StartWallpaper(idx)` | void |
| 4 | `StopWallpaper` | void |
| 5 | `LoadSettingsUI(idx)` | BSTR (JSON {schema, settings}) |
| 6 | `ApplySettings(idx, json)` | void |

## Data Flow

```
User clicks module card
  → SettingsPanel slides in (React state)
  → NativeBridge.LoadSettingsUI(idx) → returns {schema, settings}
  → SettingsControl renders recursive form

User edits settings, clicks "Apply"
  → NativeBridge.ApplySettings(idx, json)
  → writes settings.json to disk (QSaveFile)

User clicks "Start"
  → NativeBridge.StartWallpaper(idx)     [COM → Qt main thread]
  → validateIndex() → validateModuleFiles() → validateModuleSettings()
  → ensureLibraryLoaded() → m_library.load(dll)  [ModuleLibrary → LoadLibraryEx]
  → startWorker()
    → spawns m_worker thread
      → reads settings.json
      → m_library.createFn()(settings)       [DLL → createModule entry]
      → m_module.reset(factory_result)       [stores unique_ptr]
      → AttachWindowToDesktop(m_module->hwnd())
      → m_module->run()                      [blocking render loop]
  → emit runningModuleChanged → PostWebMessageAsJson → React updates button

User clicks "Stop"
  → NativeBridge.StopWallpaper()             [COM → Qt main thread]
  → DetachWindowFromDesktop()
  → m_module->stop()                         [sets atomic running=false]
  → m_worker.join()                          [waits for render loop exit]
  → m_module.reset()                         [destroys module]
  → restoreWallpaper()
```

## Threading Model

```
┌─ Qt Main Thread ───────────────────────────────────────────┐
│ QApplication event loop, WebView2Host, NativeBridge        │
│ WallpaperController: stopWallpaper(), startWallpaper(),    │
│   applySettings()                                          │
│ m_module->stop(), m_module->hwnd() (cross-thread, safe)    │
│ NEVER: glfwInit, glfwCreateWindow, OpenGL calls            │
└────────────────────────────────────────────────────────────┘
                          │
                          │ m_worker (std::thread)
                          ▼
┌─ Worker Thread ────────────────────────────────────────────┐
│ Module creation: glfwInit(), glfwCreateWindow()            │
│ Module execution: Application::mainLoop()                  │
│   → while(running) { update; render; swap; poll; }         │
│ Module destruction: ~Application(), glfwTerminate()        │
│ ALL GLFW/OpenGL calls MUST happen here                     │
└────────────────────────────────────────────────────────────┘

┌─ WebView2 Threads (managed internally) ────────────────────┐
│ NativeBridge::Invoke() callbacks                           │
│ → marshaled to main thread via QMetaObject::invokeMethod   │
│ PostWebMessageAsJson is thread-safe                        │
└────────────────────────────────────────────────────────────┘
```

**Critical rule:** GLFW initialization/teardown and OpenGL operations must happen on the **worker thread** where the GL context is current.

## Resource Ownership

| Resource | Owner | Lifetime |
|---|---|---|
| `IWallpaperModule*` | `WallpaperController::m_module` (`unique_ptr`) | From `startWorker()` to `stopWallpaper()` / worker exit |
| `HMODULE` (DLL) | `ModuleLibrary::m_lib` | From `load()` to `unload()` or destructor |
| `Settings` singleton | Function-local static in DLL | From first `Instance()` call until DLL unload |
| `GLFWwindow*` | Module's Window class | Lifetime of `Application` |
| OpenGL objects (VAO, VBO, shaders, FBO) | Module's Renderer class | RAII — destroyed with `Application` |
| Worker thread | `WallpaperController::m_worker` | From `startWorker()` to `join()`/`detach()` |
| WebView2 environment | `WebView2Host::m_env` (COM pointer) | From init callback to destructor |
| NativeBridge | `WebView2Host::m_bridge` (COM pointer) | From init callback to destructor |
| Original wallpaper path | `WallpaperController::m_originalWallpaper` | Session lifetime |
