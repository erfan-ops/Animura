# Build System

Animura uses **CMake** (3.21+) with **MSVC 2022** on Windows.

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
set(CMAKE_AUTOUIC ON)      # Auto-compile .ui files
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Qml Quick ...)
```

### Source Files
```cmake
set(ANIMURA_SOURCES
    src/main.cpp
    src/JsonUtils.cpp
    src/ModuleCatalog.cpp
    src/ModuleLibrary.cpp
    src/SettingsSchemaValidator.cpp
    src/WallpaperController.cpp
)
```

### MOC Headers (Qt meta-object compiler required)
```cmake
set(ANIMURA_MOC_HEADERS
    include/animura/ColorDialogHelper.hpp
    include/animura/WallpaperController.hpp
)
```
Only headers with `Q_OBJECT` / `QML_ELEMENT` macros need MOC. `IWallpaperModule.hpp` and utility headers do not.

### Dependencies
```cmake
target_link_libraries(Animura PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick ...
    Ole32 Dwmapi
    $<$<CONFIG:Debug>:wallpaper_host_static_mtd>
    $<$<NOT:$<CONFIG:Debug>>:wallpaper_host_static_mt>
)
```
- `Ole32`, `Dwmapi` — COM and DWM for accent color and desktop operations
- `wallpaper_host_static_mt(d)` — static library in `lib/` providing `desktop_utils.hpp`

### Compiler Options (MSVC)
```cmake
target_compile_options(Animura PRIVATE /W3 /sdl /permissive- /MP)
```
- `/W3` — warning level 3
- `/sdl` — Security Development Lifecycle checks
- `/permissive-` — standards conformance mode
- `/MP` — multi-process compilation

### Linker Options
```cmake
# Debug: console subsystem (for printf debugging)
# Release: Windows subsystem (no console window)
"$<$<CONFIG:Debug>:/SUBSYSTEM:CONSOLE>"
"$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>"
```

### Post-Build
```cmake
# windeployqt — copies Qt DLLs, plugins, QML modules to output
add_custom_command(TARGET Animura POST_BUILD
    COMMAND windeployqt --$<CONFIG> --qmldir "${CMAKE_SOURCE_DIR}/qml" ...

# Copy modules/ directory to output
add_custom_command(TARGET Animura POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/modules
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/modules
)
```

---

## Module Build: `CMakeLists.txt` (per module)

**Example:** `E:/coding/C/live-wallpaper-modules/star-simulator/CMakeLists.txt`

### Key Differences from Host

| Setting | Host | Module |
|---|---|---|
| C++ Standard | C++20 | C++23 |
| Build type | Executable | Shared library (DLL) |
| Qt dependency | Qt 6 (8+ modules) | None |
| Graphics | None (host is GUI) | OpenGL + GLFW |
| Shader embedding | N/A | Custom cmake script |
| Package manager | None | vcpkg |

### Module Source Layout
```cmake
set(APP_SOURCES
    src/module.cpp
    src/settings.cpp
    src/star.cpp
    src/shader_utils.cpp
    src/application.cpp
    src/renderer.cpp
    src/star_system.cpp
    src/raii.cpp
    include/glad/glad.c             # OpenGL loader (compiled as C)
)

add_library(module SHARED ${APP_SOURCES})
set_target_properties(module PROPERTIES
    OUTPUT_NAME "module"
    PREFIX ""                        # No "lib" prefix on Windows
    SUFFIX ".dll"
)
```

### Module Dependencies
```cmake
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)  # From vcpkg
target_link_libraries(module PRIVATE OpenGL::GL glfw)
```

### Shader Embedding
GLSL shaders are embedded at build time into a C++ header:
```cmake
set(SHADER_FILES
    shaders/main.vert shaders/main.frag
    shaders/bginit.vert shaders/bginit.frag
    shaders/bg.vert shaders/bg.frag
)
add_custom_command(
    OUTPUT ${GENERATED_SHADER_HEADER}
    COMMAND ${CMAKE_COMMAND} -DINPUT_FILES="..." -DOUTPUT_FILE=...
        -P ${CMAKE_SOURCE_DIR}/cmake/embed_shaders.cmake
)
```
The generated `shaders.hpp` contains `const char* main_vert = "...";` style literals.

### Optimization Flags (MSVC)
```cmake
add_compile_options(
    /arch:AVX2    # AVX2 instruction set
    /Oi           # Generate intrinsic functions
    /Ot           # Favor fast code
    /GT           # Fiber-safe TLS
    /GL           # Whole program optimization
)
```

### Module Post-Build
```cmake
# Copy resource files (module.json, schema.json, settings.json, preview.png)
add_custom_command(TARGET module POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ...)

# Copy runtime DLLs (glfw3.dll from vcpkg)
add_custom_command(TARGET module POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:module> $<TARGET_FILE_DIR:module>)
```

---

## Build Output Structure

```
build/
└── Release/ (or Debug/)
    ├── Animura.exe
    ├── Qt6Core.dll, Qt6Gui.dll, ...  (windeployqt copies)
    ├── wallpaper_host_static_mt.lib   (linked statically, not in output)
    └── modules/
        ├── star-simulator/
        │   ├── module.dll
        │   ├── glfw3.dll
        │   ├── module.json
        │   ├── schema.json
        │   ├── settings.json
        │   └── preview.jpg
        ├── fireflies/
        └── ...
```

---

## Build Commands

```bash
# Configure (uses CMakePresets.json)
cmake --preset default

# Build Release
cmake --build build --config Release

# Build Debug
cmake --build build --config Debug

# Run
./build/Release/Animura.exe
```

---

## Dependency Summary

| Dependency | Host | Module | Source |
|---|---|---|---|
| Qt 6.8 | ✓ | ✗ | System install (`C:/Qt/6.8.2/msvc2022_64`) |
| wallpaper_host | ✓ (static lib) | ✗ | `lib/` directory |
| OpenGL | ✗ | ✓ | Windows SDK |
| GLFW 3 | ✗ | ✓ | vcpkg (`x64-windows-static`) |
| GLAD | ✗ | ✓ | Bundled source (`include/glad/glad.c`) |
| GLM | ✗ | ✓ | Bundled headers (`include/glm/`) |
| nlohmann/json | ✓ | ✓ | Bundled headers (`include/nlohmann/json.hpp`) |

---

## CRT Consistency

Both host and modules use:
```cmake
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
```

This produces:
- `/MT` for Release — static CRT, no `vcruntime140.dll` dependency
- `/MTd` for Debug — static debug CRT

**A mismatch would cause heap corruption:** If host uses `/MT` and module uses `/MD`, `delete` in the host would call the host's CRT heap, but the object was allocated by the module's CRT heap. This is a guaranteed crash.
