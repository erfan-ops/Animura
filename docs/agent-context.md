# AI Agent Context — Animura

> **Purpose:** Fast-loading reference for AI coding agents. Read this before operating on the codebase.

## Quick Reference

### Project Identity
- **Name:** Animura — modular live wallpaper engine for Windows
- **Language:** C++20 (host), TypeScript + React 18 (frontend)
- **Build:** CMake 3.30+, MSVC 2022, static CRT (`/MT`), Vite 5
- **Platform:** Windows only (Win32 + OpenGL + GLFW + WebView2)

### Key Directories
```
Animura/
├── src/                    # Host C++ sources
├── include/animura/        # Host headers
├── include/wallpaper-host/ # Desktop integration (static lib)
├── frontend/               # React + TypeScript UI (Vite)
│   └── src/
│       ├── components/     # React components
│       ├── bridge/         # C++ ↔ JS communication layer
│       ├── hooks/          # Custom React hooks
│       └── theme/          # CSS design tokens
├── modules/                # Wallpaper plugin DLLs (8 modules)
├── resources/              # .qrc (icon only), .rc, icon.ico
└── lib/                    # wallpaper_host static libs
```

Module sources live separately at `E:/coding/C/live-wallpaper-modules/<name>/`.

### Key Classes

| Class | File | Role |
|---|---|---|
| `WallpaperController` | `src/WallpaperController.cpp` | Central orchestrator — module lifecycle, worker thread |
| `WebView2Host` | `src/WebView2Host.cpp` | Embeds Edge WebView2 in a QWidget, hosts React UI |
| `NativeBridge` | `src/NativeBridge.cpp` | COM `IDispatch` object — JS ↔ C++ communication |
| `ModuleCatalog` | `src/ModuleCatalog.cpp` | Scans `/modules` for `module.json` files |
| `ModuleLibrary` | `src/ModuleLibrary.cpp` | Wraps `LoadLibraryEx`/`FreeLibrary`/`GetProcAddress` |
| `IWallpaperModule` | `include/animura/IWallpaperModule.hpp` | ABI interface: `run()`, `stop()`, `hwnd()`, virtual dtor |
| `SettingsSchemaValidator` | `src/SettingsSchemaValidator.cpp` | Recursive JSON validation against schema |

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

4. **WebView2 threading:**
   - WebView2 callbacks arrive on arbitrary threads.
   - `NativeBridge::Invoke()` marshals all calls to the Qt main thread via `QMetaObject::invokeMethod` with `Qt::BlockingQueuedConnection`.
   - `PostWebMessageAsJson` is thread-safe.

5. **COM bridge return types:**
   - All NativeBridge methods return `BSTR` (COM string). JS receives strings, not numbers.
   - `getRunningModuleId()` returns a string like `"0"` — must be parsed with `Number()` in JS.
   - Strict equality (`===`) between string and number always fails.

### Threading Quick Reference

```
Main Thread (Qt):        UI events, WallpaperController public methods,
                         WebView2Host, NativeBridge marshaling,
                         m_module->stop(), m_module->hwnd()
WebView2 Threads:        Managed internally — callbacks arrive here,
                         marshaled to main thread via NativeBridge
Worker Thread (m_worker): Module creation, glfwInit, glfwCreateWindow,
                         mainLoop(), OpenGL, glfwTerminate, module destruction
```

### Module Interface (ABI Contract)

```cpp
class IWallpaperModule {
public:
    virtual ~IWallpaperModule() = default;
    virtual void run() = 0;
    virtual void stop() = 0;
    virtual HWND hwnd() const = 0;
};

extern "C" __declspec(dllexport) IWallpaperModule* createModule(const char* settingsJson);
```

### What NOT to Assume

- ❌ Do NOT assume `m_module.reset()` is safe to call from any thread
- ❌ Do NOT assume GLFW functions are thread-safe (they have strict thread affinity)
- ❌ Do NOT assume `stop()` blocks until `run()` returns
- ❌ Do NOT assume the module DLL is unloaded immediately after `m_module.reset()`
- ❌ Do NOT assume OpenGL objects can be deleted without a current context
- ❌ Do NOT assume all modules use the same CRT or compiler settings
- ✅ The host uses static CRT `/MT` — verify modules match

### Build Commands

```bash
# Frontend (run before C++ build)
cd frontend && npm run build

# C++ — configure
cmake --preset vs2022-x64

# C++ — build
cmake --build build --config Release
```

### Runtime Flow Summary

```
App Start → main.cpp → QApplication → WebView2Host (creates QWidget + WebView2)
  → WallpaperController (ctor scans /modules)
  → WebView2 navigates to frontend/dist/index.html (production) or localhost:5173 (dev)
  → React renders module grid via NativeBridge.GetModulesList()

User clicks module card → SettingsPanel slides in
User clicks Start:
  → NativeBridge.StartWallpaper(idx) [COM → Qt main thread]
  → validate → ensureLibraryLoaded (LoadLibraryEx)
  → startWorker → spawn thread
    → createModule(json) → m_module.reset(ptr)
    → AttachWindowToDesktop
    → m_module->run() [BLOCKS]
  → emit runningModuleChanged → PostWebMessageAsJson → React updates button

User clicks Stop:
  → NativeBridge.StopWallpaper() [COM → Qt main thread]
  → DetachWindowFromDesktop
  → m_module->stop() [sets atomic, returns immediately]
  → m_worker.join() [waits for run() to return]
  → m_module.reset() [destroys module]
  → restoreWallpaper()
```
