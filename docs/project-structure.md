# Project Structure

## Repository Tree

```
Animura/                              # Repository root
├── CMakeLists.txt                    # Host build configuration
├── CMakePresets.json                 # CMake presets (config + build)
├── README.md                         # Project README
├── PROMPT.md                         # AI documentation sync task
├── LICENSE.txt                       # MIT License
│
├── src/                              # Host C++ sources
│   ├── main.cpp                      # Entry point, single-instance, tray icon
│   ├── WallpaperController.cpp       # Module lifecycle orchestrator
│   ├── WebView2Host.cpp              # WebView2 control in QWidget, navigation
│   ├── NativeBridge.cpp              # COM IDispatch — JS ↔ C++ bridge
│   ├── ModuleCatalog.cpp             # Module discovery from /modules
│   ├── ModuleLibrary.cpp             # DLL loading wrapper
│   ├── SettingsSchemaValidator.cpp   # JSON schema validation
│   └── JsonUtils.cpp                 # JSON file I/O helper
│
├── include/                          # Host headers
│   ├── animura/                      # Public API headers
│   │   ├── IWallpaperModule.hpp      # Module interface (ABI contract)
│   │   ├── WallpaperController.hpp   # Controller (Q_OBJECT)
│   │   ├── WebView2Host.hpp          # WebView2 host (Q_OBJECT)
│   │   ├── NativeBridge.hpp          # COM IDispatch bridge
│   │   ├── ModuleCatalog.hpp         # Module discovery result
│   │   ├── ModuleInfo.hpp            # Per-module metadata struct
│   │   ├── ModuleLibrary.hpp         # DLL loader + CreateModuleFn typedef
│   │   ├── SettingsSchemaValidator.hpp
│   │   ├── JsonUtils.hpp
│   │   └── resource.h                # Windows resource IDs
│   │
│   ├── wallpaper-host/               # Desktop integration (static lib)
│   │   ├── desktop_utils.hpp         # Attach/Detach/SetWallpaper API
│   │   └── tray_utils.hpp            # System tray utilities
│   │
│   └── nlohmann/                     # Bundled JSON library
│       └── json.hpp                  # nlohmann/json (single-header)
│
├── frontend/                         # React + TypeScript UI (Vite)
│   ├── package.json                  # npm dependencies + scripts
│   ├── tsconfig.json                 # TypeScript configuration
│   ├── vite.config.ts                # Vite build config
│   ├── index.html                    # HTML entry point
│   └── src/
│       ├── main.tsx                  # React entry point
│       ├── App.tsx                   # Root layout + hook wiring
│       ├── bridge/
│       │   └── native.ts             # Typed wrapper around COM NativeBridge
│       ├── components/
│       │   ├── Header.tsx            # App header with Stop button
│       │   ├── ModuleGrid.tsx        # Responsive grid of ModuleCards
│       │   ├── ModuleCard.tsx        # Neumorphic preview card
│       │   ├── SettingsPanel.tsx     # Slide-in settings drawer
│       │   ├── SettingsControl.tsx   # Recursive form generator
│       │   ├── ColorPicker.tsx       # Full color picker (HSV, alpha, eye dropper)
│       │   └── Notification.tsx      # Toast notification system
│       ├── hooks/
│       │   ├── useBackend.ts         # useModules, useRunningModuleId, useSettings, useStartStop
│       │   └── useNotifications.ts   # Toast notification state
│       ├── theme/
│       │   └── tokens.css            # CSS custom properties (design tokens)
│       └── types/
│           └── index.ts              # TypeScript interfaces
│
├── modules/                          # Wallpaper plugin DLLs
│   ├── black-hole/
│   │   ├── module.dll, module.json, schema.json, settings.json, preview.png
│   ├── delaunay-flow/
│   ├── eclipse-frame/
│   ├── fireflies/
│   ├── hypercube-harmony/
│   ├── infinite-mirror/
│   ├── shahr-flow/
│   ├── star-simulator/
│   └── video/
│
├── resources/                        # Application resources
│   ├── resources.qrc                 # Qt Resource Collection (icon only)
│   ├── Animura.rc                    # Windows resource (app icon)
│   └── icon.ico                      # Application icon
│
├── cmake/                            # CMake helper scripts
│   └── build-frontend.cmake          # npm build invocation script
│
├── lib/                              # Prebuilt static libraries
│   ├── wallpaper_host_static_mt.lib  # Release desktop integration
│   └── wallpaper_host_static_mtd.lib # Debug desktop integration
│
├── build/                            # Build output (generated, not committed)
│   ├── Release/
│   │   ├── Animura.exe
│   │   ├── Qt6*.dll                  # windeployqt output
│   │   ├── frontend/dist/            # Copied React build
│   │   └── modules/                  # Copied from source modules/
│   └── Debug/
│
├── docs/                             # Documentation for developers
│   ├── agent-context.md
│   ├── architecture.md
│   ├── host.md
│   ├── modules.md
│   ├── example-module.md
│   ├── build-system.md
│   ├── project-structure.md
│   └── runtime.md
│
└── ai-docs/                          # Documentation for AI coding agents
    ├── README.md
    ├── architecture.md
    ├── application-lifecycle.md
    ├── asset-system.md
    ├── build-system.md
    ├── coding-conventions.md
    ├── common-ai-tasks.md
    ├── configuration.md
    ├── debugging.md
    ├── dependencies.md
    ├── development-workflow.md
    ├── graphics-system.md
    ├── native-bridge.md
    ├── performance.md
    ├── project-overview.md
    ├── react-frontend.md
    ├── rendering-pipeline.md
    ├── troubleshooting.md
    ├── ui-migration.md
    ├── ui-system.md
    ├── wallpaper-engine.md
    ├── webview2-architecture.md
    └── modules/
```

## Entry Points

| Component | Entry Point | Type |
|---|---|---|
| Host application | `src/main.cpp` → `main()` | Standard C++ entry |
| Module DLL | `src/module.cpp` → `createModule()` | `extern "C"` DLL export |
| React frontend | `frontend/src/main.tsx` → `ReactDOM.createRoot` | React 18 entry |
| Qt Resources | `resources/resources.qrc` prefix `/icons` | Compiled into binary (icon only) |
| WebView2 navigation | `https://animura.app/index.html` (production) or `http://localhost:5173` (dev) | Virtual host or dev server |

## Removed Files (post-WebView2 migration)

These files existed in the previous QML-based UI and have been removed:

| Removed | Replacement |
|---|---|
| `qml/main.qml` | `App.tsx` + `ModuleGrid.tsx` |
| `qml/components/WallpaperCard.qml` | `ModuleCard.tsx` |
| `qml/components/SettingsPanel.qml` | `SettingsPanel.tsx` |
| `qml/components/SettingsGroup.qml` | `SettingsControl.tsx` |
| `qml/components/NotificationManager.qml` | `Notification.tsx` |
| `qml/components/NotificationBanner.qml` | `Notification.tsx` |
| `resources/qml.qrc` | `resources/resources.qrc` (icon only) |
| `include/animura/ColorDialogHelper.hpp` | `ColorPicker.tsx` |

## Key File Purposes

| File | Why it exists |
|---|---|
| `IWallpaperModule.hpp` | ABI contract — identical between host and all modules |
| `WallpaperController.hpp` | Central orchestrator — owns module lifecycle, thread, and DLL |
| `WebView2Host.hpp` | Embeds WebView2 in a QWidget, hosts React frontend |
| `NativeBridge.hpp` | COM IDispatch bridge — JS ↔ C++ communication |
| `ModuleLibrary.hpp` | Isolates Win32 `LoadLibrary`/`FreeLibrary` from business logic |
| `ModuleCatalog.hpp` | Decouples filesystem scanning from module execution |
| `SettingsSchemaValidator.hpp` | Validates user settings before passing to module (fail-fast) |
| `desktop_utils.hpp` | Win32 desktop manipulation (WorkerW attachment, wallpaper save/restore) |
| `SettingsControl.tsx` | Dynamic form generation — no per-module UI code needed |
| `ColorPicker.tsx` | Full-featured color picker with HSV, alpha, presets, eye dropper |
| `native.ts` | Typed bridge wrapper — handles BSTR→number coercion |
