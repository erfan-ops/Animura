# Module System

Animura wallpaper modules are **DLL-based plugins** that implement the `IWallpaperModule` interface. Each module renders live animated content directly onto the Windows desktop via OpenGL.

## Interface Contract

**File:** `include/animura/IWallpaperModule.hpp` (host and module copies must be **byte-identical**)

```cpp
class IWallpaperModule {
public:
    virtual ~IWallpaperModule() = default;  // Virtual destructor — required for DLL boundary
    virtual void run() = 0;                // Blocking — starts the render loop
    virtual void stop() = 0;               // Non-blocking — signals render loop to exit
    virtual HWND hwnd() const = 0;         // Win32 window handle for desktop integration
};
```

### ABI Stability Requirements
- The interface header is **compiled into both host and module**.
- Must remain **byte-identical** between host and module to ensure vtable compatibility.
- Both host and modules use **static CRT (`/MT`)** — a mismatch would cause heap corruption on `delete` across the DLL boundary.
- Compiler: MSVC 2022, C++20 standard.

---

## DLL Entry Point

Every module DLL must export exactly one function:

```cpp
extern "C" __declspec(dllexport)
IWallpaperModule* createModule(const char* settingsJson);
```

- **Name:** `createModule` (resolved via `GetProcAddress`)
- **Calling convention:** `__cdecl` (default for `extern "C"`)
- **Parameter:** JSON string of module settings (from `settings.json`)
- **Return:** Heap-allocated `IWallpaperModule*` — **ownership transfers to caller**
- **Error handling:** Return `nullptr` on failure, or throw (caught by host's try/catch)

The host calls this from the worker thread, then stores the result in `m_module` (`unique_ptr<IWallpaperModule>`).

---

## Module Directory Structure

Each module lives in a subdirectory of `./modules/`:

```
modules/
└── star-simulator/
    ├── module.dll       # The compiled plugin
    ├── module.json      # Metadata (name, version, entry point)
    ├── schema.json      # JSON Schema for configurable settings
    ├── settings.json    # Current user settings
    ├── preview.png      # Thumbnail shown in module grid (loaded via https://animura.modules/...)
    └── glfw3.dll        # GLFW runtime (bundled per module)
```

### `module.json` Format

```json
{
    "name": "Star Simulator",
    "version": "1.0.0",
    "entry": "module.dll",
    "schema": "schema.json",
    "settings": "settings.json",
    "preview": "preview.png"
}
```

### `schema.json` Format

Defines what settings the module accepts. Supports nested objects, typed fields, and validation constraints. See the [Settings System](#settings-system) section below.

### `settings.json` Format

User-facing settings values that conform to `schema.json`. Written by the host when the user clicks "Apply" in the settings panel.

---

## Module Lifecycle Contract

```
┌──────────────────────────────────────────────────────────────────┐
│                         HOST THREAD (main)                        │
│                                                                   │
│  startWallpaper(idx)                                              │
│    → ensureLibraryLoaded()                                        │
│    → startWorker()                                                │
│                                                                   │
│  stopWallpaper()                                                  │
│    → DetachWindowFromDesktop()                                    │
│    → m_module->stop()         ← sets atomic, returns immediately  │
│    → m_worker.join()          ← blocks until run() returns        │
│    → m_module.reset()         ← destroys module (WRONG THREAD!)   │
│    → restoreWallpaper()                                           │
└──────────────────────────────────────────────────────────────────┘
                          │                        ▲
                          │ spawns                 │ joins
                          ▼                        │
┌──────────────────────────────────────────────────────────────────┐
│                       WORKER THREAD                               │
│                                                                   │
│  createModule(settingsJson)                                       │
│    → new Application(settings)                                    │
│      → GlfwContext()          ← glfwInit()                        │
│      → Window(msaa)           ← glfwCreateWindow()                │
│      → Renderer()             ← OpenGL shader compilation, VAO... │
│                                                                   │
│  m_module->run()                                                  │
│    → running = true                                               │
│    → mainLoop()                                                   │
│      → while(running) {                                           │
│          update; render; glfwSwapBuffers; glfwPollEvents;         │
│        }                                                          │
│    → returns when stop() sets running = false                     │
│                                                                   │
│  [module should be destroyed here, on the worker thread]          │
└──────────────────────────────────────────────────────────────────┘
```

### Phase 1: Creation (Worker Thread)
- `createModule(json)` is called
- `Application` constructor runs:
  - `GlfwContext` → `glfwInit()`
  - `Window` → `glfwCreateWindow()`, `glfwMakeContextCurrent()`, `gladLoadGLLoader()`
  - OpenGL objects: VAO, VBO, shader programs, framebuffers, textures
- All GLFW/GL state is bound to the **worker thread**

### Phase 2: Execution (Worker Thread)
- `run()` is called → `running = true` → `mainLoop()`
- Render loop: update simulation → upload geometry → render → swap buffers → poll events → tick
- The loop checks `running` (atomic) each iteration
- `glfwSwapBuffers` presents the frame to the desktop-embedded window

### Phase 3: Stop Signal (Main Thread → Worker Thread)
- `stop()` is called from the main thread
- Sets `running = false` (atomic store)
- Returns immediately — does NOT wait for `mainLoop()` to exit
- The host then calls `m_worker.join()` to block until `run()` actually returns

### Phase 4: Destruction (MUST be Worker Thread)
- `m_module.reset()` calls `delete` on the `IWallpaperModule*`
- Virtual dispatch to `Application::~Application()` (compiler-generated)
- Members destroyed in **reverse declaration order**:
  1. `Renderer` — `glDelete*` (context still active ✓)
  2. StarSystem — trivial
  3. `Window` — `glfwDestroyWindow()` (destroys window and GL context)
  4. `GlfwContext` — `glfwTerminate()` (shuts down GLFW)
- **All destruction calls must happen on the worker thread** where GLFW was initialized

---

## Settings System

### Schema Types

| Type | React Control | Description |
|---|---|---|
| `"int"` | Slider (integer step) | Integer value with optional min/max |
| `"float"` | Slider (decimal step) | Float value with optional min/max |
| `"bool"` | Toggle switch | Boolean on/off |
| `"select"` | Custom dropdown | Value from `options` array |
| `"color"` | ColorPicker (HSV, alpha, presets, eye dropper) | RGBA array `[r, g, b, a]` (0–1 range) |
| `"color_list"` | Color grid with add/remove | Array of RGBA arrays |
| Object (no `"type"`) | Nested `SettingsControl` | Recursive settings group |

### Example Schema

```json
{
    "fps": {
        "type": "int",
        "min": 10,
        "max": 144
    },
    "vsync": {
        "type": "bool"
    },
    "stars": {
        "count": { "type": "int", "min": 1, "max": 500 },
        "radius": { "type": "float", "min": 1, "max": 50 },
        "color": { "type": "color" }
    }
}
```

### Validation
- `SettingsSchemaValidator::validate()` checks all constraints before module loading.
- Type mismatches, min/max violations, invalid options, and missing keys are reported.
- Validation errors block module start.

---

## Common Pitfalls for Module Developers

### 1. Thread Affinity
All GLFW and OpenGL calls must happen on the thread that called `glfwInit()`. Do not call GL functions from `stop()` (called from main thread) or from the destructor if destruction happens on the wrong thread.

### 2. `stop()` Must Be Non-Blocking
`stop()` is called from the main thread. It must return quickly — typically just set an atomic flag. Do not join threads or call blocking functions in `stop()`.

### 3. Settings Reference Lifetime
`Application` stores `Settings& settings_`. The `Settings` singleton lives for the DLL's entire loaded lifetime, so the reference is safe. But do not copy or store the reference beyond `Application`'s lifetime.

### 4. Static CRT Required
Link with static CRT (`/MT` or `/MTd`). Using `/MD` will cause heap corruption when the host deletes the module object across the DLL boundary.

### 5. Virtual Destructor
Always declare `~IWallpaperModule() = default` (or define it). The virtual destructor is essential for correct polymorphic deletion across the DLL boundary.
