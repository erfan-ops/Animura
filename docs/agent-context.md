# AI Agent Context — Animura

> **Purpose:** Fast-loading reference for AI coding agents. Read this before operating on the codebase.

## Quick Reference

### Project Identity
- **Name:** Animura — modular live wallpaper engine for Windows
- **Language:** C++20, Qt 6 QML
- **Build:** CMake, MSVC 2022, static CRT (`/MT`)
- **Platform:** Windows only (Win32 + OpenGL + GLFW)

### Key Directories
```
Animura/
├── src/                    # Host C++ sources
├── include/animura/        # Host headers
├── include/wallpaper-host/ # Desktop integration (static lib)
├── qml/                    # Qt Quick UI
├── modules/                # Wallpaper plugin DLLs (8 modules)
├── resources/              # .qrc, .rc, icon
└── lib/                    # wallpaper_host static libs
```

Module sources live separately at `E:/coding/C/live-wallpaper-modules/<name>/`.

### Key Classes

| Class | File | Role |
|---|---|---|
| `WallpaperController` | `src/WallpaperController.cpp` | Central orchestrator — module lifecycle, worker thread |
| `ModuleCatalog` | `src/ModuleCatalog.cpp` | Scans `/modules` for `module.json` files |
| `ModuleLibrary` | `src/ModuleLibrary.cpp` | Wraps `LoadLibraryEx`/`FreeLibrary`/`GetProcAddress` |
| `IWallpaperModule` | `include/animura/IWallpaperModule.hpp` | ABI interface: `run()`, `stop()`, `hwnd()`, virtual dtor |
| `SettingsSchemaValidator` | `src/SettingsSchemaValidator.cpp` | Recursive JSON validation against schema |
| `Application` | `live-wallpaper-modules/*/src/application.cpp` | Module-side implementation of the interface |

### Critical Architecture Rules

1. **GLFW/OpenGL thread affinity is LAW.**
   - `glfwInit()`, `glfwCreateWindow()`, `glfwMakeContextCurrent()`, all GL calls, `glfwDestroyWindow()`, `glfwTerminate()` — ALL must happen on the **same thread**.
   - The worker thread (`m_worker`) is the GLFW thread. The Qt main thread is NOT.
   - **Never** call GLFW/GL functions from the main thread.

2. **Module lifecycle contract:**
   - `createModule(json)` → `run()` → `stop()` → `~IWallpaperModule()`
   - `stop()` only sets an atomic flag. It does not block or join.
   - `run()` is a blocking call — it returns only after `stop()` signals exit.
   - Module destruction MUST happen on the same thread that created it.

3. **DLL loading order:**
   - `LoadLibraryEx` → `GetProcAddress("createModule")` → call factory → store result
   - `m_module.reset()` destroys the module object
   - `FreeLibrary` happens later in `ModuleLibrary::unload()`
   - DLL must NOT be freed while any code inside it is executing.

4. **Settings singleton pattern (module side):**
   - `Settings::Instance(json)` is a Meyer's singleton (function-local static).
   - Lives in DLL static storage — valid for the DLL's entire loaded lifetime.
   - Constructor marked `noexcept` but can throw (loadFromJson reads JSON keys).
   - `Application` stores `Settings& settings_` — a reference to the singleton.

5. **Member destruction order (Application):**
   - Compiler-generated destructor destroys in reverse declaration order.
   - `GlfwContext` (last declared) is destroyed LAST → `glfwTerminate()` runs after everything else.
   - `Window` (third declared) destroyed before `GlfwContext` → `glfwDestroyWindow()` before `glfwTerminate()`.
   - `Renderer` destroyed before `Window` → OpenGL objects deleted while context still active. **This is correct.**

### Known Crash Risks

| Risk | Location | Trigger |
|---|---|---|
| **Cross-thread GLFW teardown** | `stopWallpaper()` line 179 | `m_module.reset()` on main thread calls `glfwTerminate()` — wrong thread |
| **OpenGL calls without context** | `~Renderer()` via `m_module.reset()` | GL objects deleted on main thread with no current context |
| **`noexcept` violation** | `Settings::Instance()` in module | First-call exception → `std::terminate()` |
| **Module called during destruction** | `m_module->hwnd()` / `m_module->stop()` | Race if worker thread resets `m_module` before main thread calls these |
| **Thread detach on self-call** | `stopWallpaper()` line 174 | If called from worker thread, thread is detached — module may outlive controller |

### Threading Quick Reference

```
Main Thread (Qt):       UI events, WallpaperController public methods,
                        m_module->stop(), m_module->hwnd()
Worker Thread (m_worker): Module creation, glfwInit, glfwCreateWindow,
                        mainLoop(), OpenGL, glfwTerminate, module destruction
```

### Module Interface (ABI Contract)

```cpp
class IWallpaperModule {
public:
    virtual ~IWallpaperModule() = default;  // Must be virtual for DLL boundary
    virtual void run() = 0;                // Blocking — starts render loop
    virtual void stop() = 0;               // Non-blocking — signals exit
    virtual HWND hwnd() const = 0;         // Returns Win32 window handle
};

// DLL exports:
extern "C" __declspec(dllexport) IWallpaperModule* createModule(const char* settingsJson);
```

### What NOT to Assume

- ❌ Do NOT assume `m_module.reset()` is safe to call from any thread
- ❌ Do NOT assume GLFW functions are thread-safe (they have strict thread affinity)
- ❌ Do NOT assume `stop()` blocks until `run()` returns
- ❌ Do NOT assume the module DLL is unloaded immediately after `m_module.reset()`
- ❌ Do NOT assume OpenGL objects can be deleted without a current context
- ❌ Do NOT assume `Settings::Instance()` won't throw (it can on first call)
- ❌ Do NOT assume all modules use the same CRT or compiler settings (check first)
- ✅ The host uses static CRT `/MT` — verify modules match

### Build Commands

```bash
# Configure (from repo root)
cmake --preset default

# Build
cmake --build build --config Release

# The build copies modules/ to the output directory automatically
```

### Runtime Flow Summary

```
App Start → main.cpp → QApplication → QQmlApplicationEngine
  → WallpaperController (ctor scans /modules)
  → QML loads → displays module grid
  
User clicks Start:
  → validate → ensureLibraryLoaded (LoadLibraryEx)
  → startWorker → spawn thread
    → createModule(json) → m_module.reset(ptr)
    → AttachWindowToDesktop
    → m_module->run() [BLOCKS]
    
User clicks Stop:
  → DetachWindowFromDesktop
  → m_module->stop() [sets atomic, returns immediately]
  → m_worker.join() [waits for run() to return]
  → m_module.reset() [destroys module — MUST be on worker thread!]
  → restoreWallpaper()
```
