# Animura

**Animura** is a modular live wallpaper engine for Windows built with **Qt 6 (QML + C++)**.  
It loads wallpaper modules from DLLs that render real‑time graphics directly onto the desktop via OpenGL, with a JSON‑driven settings UI generated automatically from schema.

Unlike traditional wallpaper changers, Animura modules can animate, react to mouse movement, and span across multiple monitors.

---

## Features

- **Live wallpaper modules** — dynamically loaded from DLLs, each running its own OpenGL render loop
- **Desktop integration** — attaches rendering windows directly behind desktop icons via `WorkerW`
- **Dynamic settings system** — JSON Schema defines types, ranges, and allowed values; QML UI auto‑generated
- **System tray support** — minimize to tray, restore, or quit
- **Windows accent color** — reads DWM colorization for Material theme
- **Wallpaper safety** — original wallpaper saved and restored when modules stop
- **Single-instance** — second launch notifies the running instance

---

## Architecture

```
┌───────────────────────────────────────────────┐
│                 Qt Main Thread                │
│  ┌──────────┐  ┌───────────────────────────┐  │
│  │ QML UI   │  │  WallpaperController      │  │
│  │ GridView │  │  ┌──────────────────────┐ │  │
│  │ Settings │  │  │ ModuleCatalog        │ │  │
│  │ Panel    │  │  │ ModuleLibrary        │ │  │
│  └──────────┘  │  │ m_module (unique_ptr)│ │  │
│                │  └──────────┬───────────┘ │  │
│                │             │ m_worker    │  │
│                │  ┌──────────▼───────────┐ │  │
│                │  │   Worker Thread      │ │  │
│                │  │  IWallpaperModule    │ │  │
│                │  │  ┌───────────────┐   │ │  │
│                │  │  │  Application  │   │ │  │
│                │  │  │  OpenGL+GLFW  │   │ │  │
│                │  │  └───────────────┘   │ │  │
│                │  └──────────────────────┘ │  │
│                └───────────────────────────┘  │
└───────────────────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────┐
│          WorkerW Desktop Layer               │
│  Module renders live wallpaper via OpenGL    │
└──────────────────────────────────────────────┘
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
    ├── preview.jpg        # Thumbnail in module grid
    └── glfw3.dll          # GLFW runtime
```

Available modules: **black‑hole, delaunay‑flow, eclipse‑frame, fireflies, hypercube‑harmony, infinite‑mirror, shahr‑flow, star‑simulator.**

---

## Settings System

Schema‑to‑UI mapping:

| Schema Type | QML Control |
|---|---|
| `"int"` / `"float"` | Slider (with min/max/step) |
| `"bool"` | Switch toggle |
| `"select"` | ComboBox (from `options` array) |
| `"color"` | Color swatch → native QColorDialog |
| `"color_list"` | Reorderable color grid |
| Nested object | Recursive settings group |

Validation (`min`, `max`, `options`, `type`) runs before module start. See example schemas in each module's `schema.json`.

---

## Build

```bash
# Prerequisites: Qt 6.8, MSVC 2022, CMake 3.21+
cmake --preset default
cmake --build build --config Release
```

- **Host:** C++20, Qt 6 QML, static CRT (`/MT`)
- **Modules:** C++23, OpenGL + GLFW (vcpkg), static CRT (`/MT`)
- Both use static CRT to prevent heap corruption across the DLL boundary

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

- **C++20** (host) / **C++23** (modules)
- **Qt 6.8** — QML, Qt Quick, Widgets, Network
- **OpenGL 3.3** — Core profile rendering
- **GLFW 3** — Window creation and input
- **Win32 API** — Desktop integration (WorkerW), DWM, tray icon
- **nlohmann/json** — JSON parsing
- **CMake** — Build system

---

## License

MIT — see [LICENSE.txt](LICENSE.txt).
