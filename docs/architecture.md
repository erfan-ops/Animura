# Architecture Overview

## System Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                      Qt Main Thread                         │
│  ┌──────────┐  ┌──────────────┐  ┌──────────────────────┐   │
│  │ main.qml │  │ SettingsPanel│  │ ColorDialogHelper    │   │
│  │ GridView │  │ (QML Drawer) │  │ (QML → QColorDialog) │   │
│  └────┬─────┘  └──────┬───────┘  └──────────┬───────────┘   │
│       │               │                     │               │
│  ┌────▼───────────────▼─────────────────────▼────────────┐  │
│  │              WallpaperController                      │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌─────────────┐   │  │
│  │  │ModuleCatalog│  │ModuleLibrary │  │m_module     │   │  │
│  │  │(discovery)  │  │(LoadLibrary) │  │(unique_ptr) │   │  │
│  │  └─────────────┘  └──────────────┘  └──────┬──────┘   │  │
│  │  ┌──────────────────────────────────────────▼──────┐  │  │
│  │  │              m_worker (std::thread)             │  │  │
│  │  │  ┌──────────────────────────────────────────┐   │  │  │
│  │  │  │        IWallpaperModule (DLL)            │   │  │  │
│  │  │  │  Application::run() → mainLoop()         │   │  │  │
│  │  │  │  (OpenGL / GLFW render loop)             │   │  │  │
│  │  │  └──────────────────────────────────────────┘   │  │  │
│  │  └─────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   WorkerW Desktop Layer                     │
│  ┌──────────────────────────────────────────────────────┐   │
│  │            Module's GLFW Window                      │   │
│  │            (Attached via AttachWindowToDesktop)      │   │
│  │            Renders live wallpaper content            │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Subsystems

### 1. Host Application (`src/`, `include/animura/`)
The Qt-based executable that manages module discovery, loading, lifecycle, and settings UI.

| Component | Responsibility |
|---|---|
| `main.cpp` | Entry point, single-instance lock, tray icon, QML engine setup |
| `WallpaperController` | Central orchestrator: load/start/stop modules, manage worker thread |
| `ModuleCatalog` | Scans `/modules` directory, parses `module.json` metadata |
| `ModuleLibrary` | Wraps `LoadLibraryEx`/`FreeLibrary`, resolves `createModule` entry point |
| `SettingsSchemaValidator` | Validates module settings JSON against its schema before loading |
| `JsonUtils` | Thin helper for reading JSON files with error reporting |
| `ColorDialogHelper` | Exposes `QColorDialog::getColor()` to QML |

### 2. Frontend (`qml/`)
Qt Quick QML UI rendered in a `QQmlApplicationEngine`.

| File | Purpose |
|---|---|
| `main.qml` | Application window, toolbar, grid of wallpaper cards |
| `WallpaperCard.qml` | Preview card with hover animation |
| `SettingsPanel.qml` | Right-edge drawer for module settings |
| `SettingsGroup.qml` | Recursive settings form generator from JSON schema |
| `NotificationManager.qml` / `NotificationBanner.qml` | Animated toast notifications |

### 3. Module Interface (`include/animura/IWallpaperModule.hpp`)
The ABI contract between host and module DLLs. Only four virtual methods:
- `~IWallpaperModule()` — virtual destructor (defaulted)
- `run()` — blocking call, starts render loop
- `stop()` — signals render loop to exit (sets atomic flag)
- `hwnd()` — returns the Win32 window handle for desktop attachment

### 4. Wallpaper Modules (`modules/`)
DLL-based plugins implementing `IWallpaperModule`. Each module ships:
- `module.dll` — the compiled plugin
- `module.json` — metadata (name, version, entry point DLL name)
- `schema.json` — JSON Schema defining configurable settings
- `settings.json` — current setting values (user-editable)
- `preview.jpg` — preview thumbnail
- `glfw3.dll` — GLFW runtime (bundled per module)

Available modules: black-hole, delaunay-flow, eclipse-frame, fireflies, hypercube-harmony, infinite-mirror, shahr-flow, star-simulator.

### 5. Desktop Integration (`include/wallpaper-host/`)
Static library providing Win32 wallpaper manipulation:
- `AttachWindowToDesktop(HWND)` — parents module window to WorkerW behind desktop icons
- `DetachWindowFromDesktop(HWND)` — restores window to top-level
- `GetCurrentWallpaperPath()` / `SetWallpaper()` — save/restore original wallpaper

## Data Flow

```
User clicks module card
  → SettingsPanel opens
  → User edits settings, clicks "Apply"
  → backend.applySettings(moduleId, settings)     [WallpaperController]
  → writes settings.json to disk

User clicks "Start"
  → backend.startWallpaper(moduleId)               [WallpaperController]
  → validateModuleFiles() → validateModuleSettings()
  → ensureLibraryLoaded() → m_library.load(dll)    [ModuleLibrary → LoadLibraryEx]
  → startWorker()
    → spawns m_worker thread
      → reads settings.json
      → m_library.createFn()(settings)             [DLL → createModule entry]
      → m_module.reset(factory_result)             [stores unique_ptr]
      → AttachWindowToDesktop(m_module->hwnd())    [desktop integration]
      → m_module->run()                            [blocking render loop]

User clicks "Stop"
  → backend.stopWallpaper()                        [WallpaperController]
  → DetachWindowFromDesktop()
  → m_module->stop()                               [sets atomic running=false]
  → m_worker.join()                                [waits for render loop exit]
  → m_module.reset()                               [destroys module]
  → restoreWallpaper()                             [restores original wallpaper]
```

## Threading Model

```
┌─ Qt Main Thread ─────────────────────────────────────┐
│ QML rendering, UI events, WallpaperController logic   │
│ stopWallpaper(), startWallpaper(), applySettings()    │
│ m_module->stop(), m_module->hwnd() (cross-thread)     │
└───────────────────────────────────────────────────────┘
                          │
                          │ m_worker (std::thread)
                          ▼
┌─ Worker Thread ───────────────────────────────────────┐
│ Module creation: glfwInit(), glfwCreateWindow()       │
│ Module execution: Application::mainLoop()             │
│   → while(running) { render; glfwSwapBuffers; }       │
│ Module destruction: ~Application(), glfwTerminate()   │
└───────────────────────────────────────────────────────┘
```

**Critical rule:** GLFW initialization/teardown and OpenGL operations must happen on the **worker thread** where the GL context is current. Cross-thread GLFW/OpenGL calls cause undefined behavior (typically access violations).

## Resource Ownership

| Resource | Owner | Lifetime |
|---|---|---|
| `IWallpaperModule*` | `WallpaperController::m_module` (`unique_ptr`) | From `startWorker()` to `stopWallpaper()` |
| `HMODULE` (DLL) | `ModuleLibrary::m_lib` | From `load()` to `unload()` or destructor |
| `Settings` singleton | Function-local static in DLL | From first `Instance()` call until DLL unload |
| `GLFWwindow*` | `Window::window_` (`unique_ptr<GLFWwindow, GlfwWindowDeleter>`) | Lifetime of `Application` |
| OpenGL objects (VAO, VBO, shaders, etc.) | `Renderer`, `VertexArray`, `ArrayBuffer`, etc. | RAII — destroyed with `Application` |
| Worker thread | `WallpaperController::m_worker` | From `startWorker()` to `join()`/`detach()` |
| Original wallpaper path | `WallpaperController::m_originalWallpaper` | Session lifetime |
