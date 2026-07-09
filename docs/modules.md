# Module System

Animura wallpaper modules are **DLL-based plugins** that implement the `IWallpaperModule` interface. Each module renders live animated content directly onto the Windows desktop.

The host application is **renderer-agnostic** — a module only needs to implement the `IWallpaperModule` interface. Modules are free to use any rendering technology, graphics library, windowing framework, or multimedia system they choose, as long as they correctly implement the required interface contract.

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
    └── ...              # Module-specific dependencies (e.g., rendering library DLLs)
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
┌───────────────────────────────────────────────────────────────────┐
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
│    → restoreWallpaper()                                           │
└───────────────────────────────────────────────────────────────────┘
                          │                        ▲
                          │ spawns                 │ joins
                          ▼                        │
┌───────────────────────────────────────────────────────────────────┐
│                       WORKER THREAD                               │
│                                                                   │
│  createModule(settingsJson)                                       │
│    → Module constructor (init rendering, create window, etc.)     │
│                                                                   │
│  m_module->run()                                                  │
│    → running = true                                               │
│    → mainLoop()                                                   │
│      → while(running) {                                           │
│          update; render; present; poll;                           │
│        }                                                          │
│    → returns when stop() sets running = false                     │
│                                                                   │
│  m_module.reset()    ← destroys module (worker thread teardown)   │
└───────────────────────────────────────────────────────────────────┘
```

### Phase 1: Creation (Worker Thread)
- `createModule(json)` is called
- The module constructor initializes its resources:
  - Windowing (e.g., GLFW, SDL, DirectX window creation)
  - Rendering context (e.g., OpenGL context, Direct3D device, Vulkan instance)
  - Graphics resources (buffers, shaders, textures, etc.)
- All per-thread state should be bound to the **worker thread**

### Phase 2: Execution (Worker Thread)
- `run()` is called → `running = true` → `mainLoop()`
- Render loop: update simulation → render frame → present to window → poll events
- The loop checks `running` (atomic) each iteration
- The frame is presented through the desktop-embedded window

### Phase 3: Stop Signal (Main Thread → Worker Thread)
- `stop()` is called from the main thread
- Sets `running = false` (atomic store)
- Returns immediately — does NOT wait for `mainLoop()` to exit
- The host then calls `m_worker.join()` to block until `run()` actually returns

### Phase 4: Destruction (Worker Thread)
- After `run()` returns, the worker lambda calls `m_module.reset()` to `delete` the `IWallpaperModule*`
- Virtual dispatch to the module's destructor
- Resources destroyed in **reverse declaration order** on the worker thread
- This ensures rendering libraries with thread affinity requirements (OpenGL/GLFW, Direct3D, Vulkan) can safely tear down their resources on the same thread that initialized them

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
| `"file"` | Browse button + path display | String — full file path selected via native OS file dialog |
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

## Module Distribution & Installation

Modules are distributed as **ZIP packages** and can be installed at runtime without restarting Animura.

### Package Format

```text
MyModule.zip

├── module.json
├── module.dll
├── settings.json
├── preview.png
└── ... (other bundled files)
```

**Requirements:**
- `module.json` must exist at the **ZIP root** (not inside a subdirectory).
- The package must contain a valid `"id"` field in `module.json` — this becomes the installation folder name.
- All standard `module.json` fields (`id`, `name`, `version`, `entry`, `schema`, `settings`, `preview`) are required.

### Installation Flow

```
User clicks "Add Module"
  → Native file dialog (filter: *.zip)
  → User selects ZIP

Validate:
  ├─ ZIP is readable
  ├─ module.json exists at ZIP root
  ├─ JSON is valid
  ├─ "id" exists and is not empty
  ├─ "id" is not already installed (duplicate check)
  └─ No unsafe entry paths (.. traversal, absolute paths)

Extract:
  └─ All files extracted to modules/<module-id>/

Update:
  ├─ ModuleCatalog gets the new ModuleInfo
  ├─ modulesChanged signal emitted
  └─ Frontend module grid refreshes automatically
```

Installation happens entirely at runtime — **no restart required**.

### Resulting Directory Structure

```
Animura.exe

modules/
├── black-hole/
│   └── ...
└── my-module-id/
    ├── module.json
    ├── settings.json
    ├── module.dll
    ├── preview.png
    └── ...
```

The folder name is the module's `"id"` from `module.json`.

### Validation Rules

| Check | Error if |
|---|---|
| ZIP readable | File cannot be opened or is not a valid ZIP |
| `module.json` exists | Entry not found in ZIP |
| `module.json` at root | Entry is inside a subdirectory |
| Valid JSON | Parsing fails |
| `"id"` field | Missing or empty |
| Duplicate ID | Another installed module already has this ID |
| All required fields | Any of `id`, `name`, `version`, `entry`, `schema`, `settings`, `preview` missing |
| Referenced files | DLL, schema, settings, or preview file missing after extraction |
| Safe entry paths | Entry name contains `..` or is an absolute path |

### Implementation

The installer uses:
- **minizip-ng** for synchronous, deterministic ZIP I/O.
- **ZipExtractor** for entry inspection (`HasEntry`, `ReadFile`), path-traversal protection, and extraction (`ExtractAll`).
- `module.json` is read **directly from the ZIP into memory** — no temp file extraction required.
- Extraction is fully synchronous and checked for errors; it does not use Shell COM, polling, or `Sleep()`.

---
## Common Pitfalls for Module Developers

### 1. Thread Affinity
Many rendering libraries (OpenGL/GLFW, Direct3D 11, Vulkan) have thread affinity requirements — initialization, rendering, and teardown must all happen on the same thread. Do not call rendering functions from `stop()` (called from main thread). The host destroys the module on the worker thread (after `run()` returns), so the destructor runs on the correct thread. Plan your module's thread model accordingly.

### 2. `stop()` Must Be Non-Blocking
`stop()` is called from the main thread. It must return quickly — typically just set an atomic flag. Do not join threads or call blocking functions in `stop()`.

### 3. Settings Reference Lifetime
`Application` stores `Settings& settings_`. The `Settings` singleton lives for the DLL's entire loaded lifetime, so the reference is safe. But do not copy or store the reference beyond `Application`'s lifetime.

### 4. Static CRT Required
Link with static CRT (`/MT` or `/MTd`). Using `/MD` will cause heap corruption when the host deletes the module object across the DLL boundary.

### 5. Virtual Destructor
Always declare `~IWallpaperModule() = default` (or define it). The virtual destructor is essential for correct polymorphic deletion across the DLL boundary.
