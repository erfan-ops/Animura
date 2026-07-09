# Build System

Animura uses **CMake 3.30+** with **MSVC 2022** on Windows. The React frontend is built separately with **Vite 5**.

---

## Host Build: `CMakeLists.txt` (root)

### Project Configuration

```cmake
project(Animura VERSION 1.2.3 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # /MT, /MTd
```

**CRT is static (`/MT`).** This is critical for DLL boundary safety — modules must also use `/MT`.

### Qt Integration
```cmake
set(CMAKE_AUTOMOC ON)      # Auto-generate MOC files
set(CMAKE_AUTORCC ON)      # Auto-compile .qrc resources
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Widgets)
```

Only the four essential Qt modules are used (QML/Quick were replaced by WebView2 + React). `CMAKE_AUTOUIC` is not needed (no `.ui` files).

### Source Files
```cmake
set(ANIMURA_SOURCES
    src/main.cpp
    src/JsonUtils.cpp
    src/ModuleCatalog.cpp
    src/ModuleLibrary.cpp
    src/SettingsSchemaValidator.cpp
    src/WallpaperController.cpp
    src/NativeBridge.cpp
    src/WebView2Host.cpp
    src/ZipExtractor.cpp
)
```

### MOC Headers
```cmake
set(ANIMURA_MOC_HEADERS
    include/animura/WallpaperController.hpp
    include/animura/WebView2Host.hpp
)
```
Only two headers need Qt MOC — both have `Q_OBJECT`. `ColorDialogHelper.hpp` was removed (replaced by React `ColorPicker`).

### WebView2 SDK
```cmake
find_package(unofficial-webview2 CONFIG REQUIRED)
target_link_libraries(Animura PRIVATE unofficial::webview2::webview2)
```
The WebView2 SDK is provided via vcpkg. The toolchain file is set in `CMakePresets.json`.

### Dependencies
```cmake
target_link_libraries(Animura PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Widgets
    Ole32
    unofficial::webview2::webview2
    MINIZIP::minizip-ng
    $<$<CONFIG:Debug>:wallpaper_host_static_mtd>
    $<$<NOT:$<CONFIG:Debug>>:wallpaper_host_static_mt>
)
```
- `Qt6::Core, Gui, Network, Widgets` — application shell, tray, single-instance
- `Ole32` — COM for desktop wallpaper operations
- `unofficial::webview2::webview2` — Edge WebView2 for hosting the React UI
- `MINIZIP::minizip-ng` — ZIP reading and extraction (module installation)
- `wallpaper_host_static_mt(d)` — static library in `lib/` providing `desktop_utils.hpp`

### Compiler Options (MSVC)
```cmake
target_compile_options(Animura PRIVATE /W3 /sdl /permissive- /MP)
```

### Post-Build
```cmake
# windeployqt — copies Qt DLLs to output (no --qmldir, no Quick imports)
add_custom_command(TARGET Animura POST_BUILD
    COMMAND windeployqt --$<CONFIG> --no-quick-import "$<TARGET_FILE:Animura>"

# Copy modules/ directory to output
add_custom_command(TARGET Animura POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/modules
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/modules

# Copy React frontend dist to output
if(EXISTS "${CMAKE_SOURCE_DIR}/frontend/dist")
    add_custom_command(TARGET Animura POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/frontend/dist"
            "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/frontend/dist"
```

---

## Frontend Build: `frontend/` (Vite + TypeScript)

### Stack
- React 18 with TypeScript
- Vite 5 for build/dev server
- CSS custom properties for design tokens

### Commands
```bash
cd frontend
npm install          # First time only
npm run build        # Production build → dist/
npm run dev          # Dev server on localhost:5173
```

### Build Output
```
frontend/dist/
├── index.html
└── assets/
    ├── index-*.css   (~1 KB)
    └── index-*.js    (~170 KB)
```

### Configuration
```typescript
// vite.config.ts
export default defineConfig({
  plugins: [react()],
  base: './',
  build: { outDir: 'dist', emptyOutDir: true },
  server: { port: 5173, strictPort: true },
});
```

---

## Module Build (separate repository)

**Location:** `E:/coding/C/live-wallpaper-modules/<name>/CMakeLists.txt`

### Key Differences from Host

| Setting | Host | Module |
|---|---|---|
| C++ Standard | C++20 | C++23 |
| Build type | Executable | Shared library (DLL) |
| Qt dependency | Qt 6 (4 modules) | None |
| WebView2 | Yes | No |
| Graphics | None (host is GUI) | Varies by module (e.g., OpenGL + GLFW) |
| Shader embedding | N/A | Custom cmake script |
| Package manager | vcpkg (WebView2 only) | vcpkg |

### Module Source Layout
```cmake
add_library(module SHARED ${APP_SOURCES})
set_target_properties(module PROPERTIES
    OUTPUT_NAME "module"
    PREFIX ""                        # No "lib" prefix on Windows
    SUFFIX ".dll"
)
```

### Module Dependencies (Example: OpenGL-based module)
```cmake
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(module PRIVATE OpenGL::GL glfw)
```

Modules are free to use any rendering technology. The build configuration above shows a typical OpenGL/GLFW module — other modules may use different libraries (DirectX, Vulkan, SDL, media frameworks, etc.) with their own `find_package` and `target_link_libraries` directives.

---

## Build Output Structure

```
build/
└── Release/ (or Debug/)
    ├── Animura.exe
    ├── Qt6Core.dll, Qt6Gui.dll, Qt6Network.dll, Qt6Widgets.dll
    ├── frontend/
    │   └── dist/
    │       ├── index.html
    │       └── assets/
    └── modules/
        ├── star-simulator/
        │   ├── module.dll
        │   ├── module.json
        │   ├── schema.json
        │   ├── settings.json
        │   └── preview.png
        └── ...
```

---

## Build Commands

```bash
# 1. Build frontend
cd frontend && npm run build

# 2. Configure (uses CMakePresets.json)
cmake --preset vs2022-x64

# 3. Build Release
cmake --build build --config Release

# 4. Run
./build/Release/Animura.exe
```

---

## Dependency Summary

The host application has the following dependencies. Modules are independent and may use completely different dependencies — different graphics APIs, different libraries, or none at all. This table documents only what the host itself requires.

| Dependency | Source |
| --- | --- |
| Qt 6.8 (Core/Gui/Network/Widgets) | System install (`C:/Qt/6.8.2/msvc2022_64`) |
| WebView2 SDK | vcpkg (`unofficial-webview2`) |
| minizip-ng 4.x | vcpkg (`MINIZIP::minizip-ng`, static CRT triplet) |
| wallpaper_host (static lib) | `lib/` directory |
| React / TypeScript / Vite | npm (`frontend/package.json`) |
| nlohmann/json | Bundled headers (`include/nlohmann/json.hpp`) |
| Ole32 | Windows SDK |

---

## CRT Consistency

Both host and modules use:
```cmake
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
```

This produces:
- `/MT` for Release — static CRT
- `/MTd` for Debug — static debug CRT

**A mismatch causes heap corruption:** objects allocated by the module's CRT heap would be freed by the host's CRT heap (or vice versa).
