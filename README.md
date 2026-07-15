# Animura

**Animura** is a modular live wallpaper engine for Windows.  
It loads wallpaper modules from DLLs that render real‑time animated graphics directly onto the desktop. Each module defines its own configurable settings, and the application's settings UI is generated dynamically for every module.

Unlike traditional wallpaper changers, Animura modules can animate, react to mouse movement, and span across multiple monitors.

---

## Features

- **Live wallpaper modules** — dynamically loaded from DLLs, each running its own render loop
- **Desktop integration** — attaches rendering windows directly behind desktop icons via `WorkerW`
- **Dynamic settings system** — each module defines its own settings; the UI is generated automatically
- **Persistent application settings** — preferences stored in `%LOCALAPPDATA%/Animura/settings.json`
- **Automatic wallpaper restore** — optionally restores the last running wallpaper on startup
- **System tray support** — minimize to tray, restore, or quit
- **Single-instance** — second launch notifies the running instance

---

## Architecture

```
┌───────────────────────────────────────────────────┐
│                  Qt Main Thread                   │
│  ┌────────────────┐   ┌────────────────────────┐  │
│  │ React frontend │   │  WallpaperController   │  │
│  │ (WebView2)     │   │  ┌───────────────────┐ │  │
│  │ Module Grid    │   │  │ ModuleCatalog     │ │  │
│  │ Settings Panel │   │  │ ModuleLibrary     │ │  │
│  └───────┬────────┘   │  │ m_module (unique) │ │  │
│          │ COM bridge │  └────────┬──────────┘ │  │
│  ┌───────▼────────┐   │           │ m_worker   │  │
│  │  NativeBridge  │   │  ┌────────▼──────────┐ │  │
│  │  (IDispatch)   │   │  │   Worker Thread   │ │  │
│  └────────────────┘   │  │  IWallpaperModule │ │  │
│                       │  └───────────────────┘ │  │
│                       └────────────────────────┘  │
└───────────────────────────────────────────────────┘
                         │
                         ▼
┌──────────────────────────────────────────────────┐
│           WorkerW Desktop Layer                  │
│   Module renders live wallpaper content          │
└──────────────────────────────────────────────────┘
```

---

## Module Interface

```cpp
class IWallpaperModule {
public:
    virtual ~IWallpaperModule() = default;
    virtual void run() = 0;                 // Blocking — starts render loop
    virtual void stop() = 0;                // Non-blocking — signals exit
    virtual HWND hwnd() const = 0;          // Win32 window for desktop attach
};

// DLL exports:
extern "C" __declspec(dllexport)
IWallpaperModule* createModule(const char* settingsJson);
```

---

## Module Directory

```
modules/
└── star-simulator/
    ├── module.dll         # Plugin DLL
    ├── module.json        # { name, version, entry, schema, settings, preview }
    ├── schema.json        # JSON Schema → auto‑generated settings UI
    ├── settings.json      # User‑editable values
    └── preview.jpg        # Thumbnail in module grid
```

Available modules: **black‑hole, delaunay‑flow, eclipse‑frame, fireflies, hypercube‑harmony, infinite‑mirror, shahr‑flow, star‑simulator, video.**

---

## Settings System

Schema‑to‑UI mapping:

| Schema Type | Control |
|---|---|
| `"int"` / `"float"` | Slider (with min/max/step) |
| `"bool"` | Switch toggle |
| `"select"` | Dropdown (from `options` array) |
| `"color"` | Color picker (HSV, alpha, presets, eye dropper) |
| `"color_list"` | Reorderable color grid |
| `"file"` | Opens a file picker to select a file from the filesystem |
| Nested object | Recursive settings group |

Validation (`min`, `max`, `options`, `type`) runs before module start. See example schemas in each module's `schema.json`.

---

## Build

```bash
# Prerequisites: Qt 6.8, MSVC 2022, CMake 3.21+
cmake --preset default
cmake --build build --config Release
```

- **Host:** C++20, Qt 6 (Widgets), WebView2, static CRT (`/MT`)
- **Modules:** Compiled as DLLs, loaded at runtime by the host
- Both host and modules use static CRT to prevent heap corruption across the DLL boundary

---

## Documentation

Full developer documentation in [`/docs`](docs/):

| Document | Covers |
|---|---|
| [architecture.md](docs/architecture.md) | System design, data flow, threading model |
| [host.md](docs/host.md) | Host application classes and API |
| [modules.md](docs/modules.md) | Module interface contract, lifecycle, pitfalls |
| [example-module.md](docs/example-module.md) | Deep walkthrough of star-simulator |
| [build-system.md](docs/build-system.md) | CMake, dependencies, output structure |
| [project-structure.md](docs/project-structure.md) | Full directory tree and file purposes |
| [runtime.md](docs/runtime.md) | Startup, load, shutdown sequences, crash analysis |
| [agent-context.md](docs/agent-context.md) | AI agent quick reference |

---

## Technology Stack

- **C++20** — host application and module interface
- **Qt 6.8** — Core, Gui, Network, Widgets
- **Microsoft Edge WebView2** — embedded browser hosting the frontend
- **React 18 + TypeScript + Vite 5** — frontend UI
- **Win32 API** — desktop integration (WorkerW), DWM, tray icon
- **CMake** — build system

---

## License

MIT — see [LICENSE.txt](LICENSE.txt).
