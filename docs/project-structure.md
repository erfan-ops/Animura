# Project Structure

## Repository Tree

```
Animura/                              # Repository root
├── CMakeLists.txt                    # Host build configuration
├── CMakePresets.json                 # CMake presets (configure defaults)
├── README.md                         # Project README
├── START_HERE.md                     # AI agent task prompt
├── LICENSE.txt                       # MIT License
│
├── src/                              # Host C++ sources
│   ├── main.cpp                      # Entry point, single-instance, tray icon
│   ├── WallpaperController.cpp       # Module lifecycle orchestrator
│   ├── ModuleCatalog.cpp             # Module discovery from /modules
│   ├── ModuleLibrary.cpp             # DLL loading wrapper
│   ├── SettingsSchemaValidator.cpp   # JSON schema validation
│   └── JsonUtils.cpp                 # JSON file I/O helper
│
├── include/                          # Host headers
│   ├── animura/                      # Public API headers
│   │   ├── IWallpaperModule.hpp      # Module interface (ABI contract)
│   │   ├── WallpaperController.hpp   # Controller (Q_OBJECT, QML_ELEMENT)
│   │   ├── ModuleCatalog.hpp         # Module discovery result
│   │   ├── ModuleInfo.hpp            # Per-module metadata struct
│   │   ├── ModuleLibrary.hpp         # DLL loader + CreateModuleFn typedef
│   │   ├── SettingsSchemaValidator.hpp
│   │   ├── JsonUtils.hpp
│   │   ├── ColorDialogHelper.hpp     # QML → QColorDialog bridge
│   │   └── resource.h                # Windows resource IDs
│   │
│   ├── wallpaper-host/               # Desktop integration (static lib)
│   │   ├── desktop_utils.hpp         # Attach/Detach/SetWallpaper API
│   │   └── tray_utils.hpp            # System tray utilities
│   │
│   └── nlohmann/                     # Bundled JSON library
│       └── json.hpp                  # nlohmann/json (single-header)
│
├── qml/                              # Qt Quick UI
│   ├── main.qml                      # Application window + toolbar + grid
│   └── components/
│       ├── WallpaperCard.qml         # Module preview card
│       ├── SettingsPanel.qml         # Right drawer for module settings
│       ├── SettingsGroup.qml         # Recursive settings form generator
│       ├── NotificationManager.qml   # Toast notification queue
│       └── NotificationBanner.qml    # Animated toast banner
│
├── modules/                          # Wallpaper plugin DLLs (8 modules)
│   ├── black-hole/                   # Each module:
│   │   ├── module.dll                #   Compiled plugin
│   │   ├── module.json               #   Metadata manifest
│   │   ├── schema.json               #   Settings schema
│   │   ├── settings.json             #   User settings
│   │   ├── preview.jpg               #   Thumbnail
│   │   └── glfw3.dll                 #   GLFW runtime
│   ├── delaunay-flow/
│   ├── eclipse-frame/
│   ├── fireflies/
│   ├── hypercube-harmony/
│   ├── infinite-mirror/
│   ├── shahr-flow/
│   └── star-simulator/
│
├── resources/                        # Qt resources
│   ├── qml.qrc                       # Qt Resource Collection (QML files)
│   ├── Animura.rc                    # Windows resource (icon)
│   └── icon.ico                      # Application icon
│
├── lib/                              # Prebuilt static libraries
│   ├── wallpaper_host_static_mt.lib  # Release desktop integration
│   └── wallpaper_host_static_mtd.lib # Debug desktop integration
│
├── build/                            # Build output (generated, not committed)
│   ├── Release/
│   │   ├── Animura.exe
│   │   ├── Qt6*.dll                  # windeployqt output
│   │   └── modules/                  # Copied from source modules/
│   └── Debug/
│
└── docs/                             # Documentation (this directory)
    ├── architecture.md
    ├── host.md
    ├── modules.md
    ├── example-module.md
    ├── build-system.md
    ├── project-structure.md
    ├── runtime.md
    └── agent-context.md
```

---

## Module Source Repositories

Module source code lives in a **separate repository** to keep the host and modules independent:

```
E:/coding/C/live-wallpaper-modules/
├── star-simulator/
│   ├── CMakeLists.txt
│   ├── headers/
│   │   ├── application.hpp
│   │   ├── raii.hpp
│   │   ├── renderer.hpp
│   │   ├── settings.hpp
│   │   ├── star.hpp
│   │   ├── star_system.hpp
│   │   ├── shader_utils.hpp
│   │   ├── types.hpp
│   │   └── IWallpaperModule.hpp     # Copy of host interface
│   ├── src/
│   │   ├── module.cpp               # DLL entry point
│   │   ├── application.cpp
│   │   ├── raii.cpp
│   │   ├── renderer.cpp
│   │   ├── settings.cpp
│   │   ├── star.cpp
│   │   ├── star_system.cpp
│   │   └── shader_utils.cpp
│   ├── shaders/                     # GLSL shader source
│   ├── include/                     # Bundled libs (glad, glm, glfw headers)
│   ├── resource/                    # module.json, schema.json, settings.json, preview
│   └── vcpkg_installed/             # vcpkg dependencies
│
├── fireflies/                       # Similar structure
├── hypercube-harmony/
├── infinity-mirror/
├── delaunay-flow/
├── eclipse-frame/
├── shahr-flow/
└── black-hole/
```

---

## Entry Points

| Component | Entry Point | Type |
|---|---|---|
| Host application | `src/main.cpp` → `main()` | Standard C++ entry |
| Module DLL | `src/module.cpp` → `createModule()` | `extern "C"` DLL export |
| QML UI | `qml/main.qml` → `ApplicationWindow` | Qt Quick root |
| Qt Resources | `resources/qml.qrc` prefix `/qt/qml/animura` | Compiled into binary |

---

## Key File Purposes

| File | Why it exists |
|---|---|
| `IWallpaperModule.hpp` | ABI contract — must be identical between host and all modules |
| `WallpaperController.hpp` | Central orchestrator — owns module lifecycle, thread, and DLL |
| `ModuleLibrary.hpp` | Isolates Win32 `LoadLibrary`/`FreeLibrary` from business logic |
| `ModuleCatalog.hpp` | Decouples filesystem scanning from module execution |
| `SettingsSchemaValidator.hpp` | Validates user settings before passing to module (fail-fast) |
| `desktop_utils.hpp` | Win32 desktop manipulation (WorkerW attachment, wallpaper save/restore) |
| `ColorDialogHelper.hpp` | Minimal Qt/QML bridge for native color picker dialog |
| `SettingsGroup.qml` | Dynamic form generation — no per-module QML needed |
