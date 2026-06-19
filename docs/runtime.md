# Runtime & Lifecycle Documentation

## Startup Sequence

```
1. main()
   ├─ SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)
   │   └─ DLL search security: restricts to system dirs only
   │
   ├─ QApplication app(argc, argv)
   │
   ├─ QLockFile("Animura.lock")
   │   ├─ If locked → notify existing instance → exit
   │   └─ If unlocked → continue
   │
   ├─ QQmlApplicationEngine engine
   ├─ WallpaperController wallpaperController
   │   └─ ModuleCatalog("modules") → scans ./modules/ directory
   │       └─ Reads each subdirectory's module.json
   │       └─ Validates all required files exist
   │       └─ Populates m_modules vector
   │
   ├─ ColorDialogHelper colorDialogHelper
   ├─ engine.rootContext()->setContextProperty(...)
   │   ├─ "backend" → &wallpaperController
   │   ├─ "ColorDialogHelper" → &colorDialogHelper
   │   └─ "WindowsAccent" → getWindowsAccentColor()
   │
   ├─ engine.load("qrc:/qt/qml/animura/main.qml")
   │
   ├─ QLocalServer → listens for "show" from other instances
   ├─ connect(aboutToQuit → stopWallpaper)
   ├─ setupTrayIcon()
   │
   └─ app.exec() → Qt event loop
```

---

## Module Loading Sequence

```
User clicks "Start" in SettingsPanel
  → backend.startWallpaper(moduleIndex)

1. validateIndex(moduleIndex)
   └─ Bounds check on m_catalog.modules()

2. validateModuleFiles(info)
   └─ std::filesystem::exists(dll_path)
   └─ std::filesystem::exists(schema_path)
   └─ std::filesystem::exists(settings_path)

3. validateModuleSettings(info)
   └─ Read schema.json → parse JSON
   └─ Read settings.json → parse JSON
   └─ SettingsSchemaValidator::validate(schema, settings, errors)
      ├─ Type checks (int, float, bool, string)
      ├─ Range checks (min, max)
      ├─ Option checks (allowed values)
      └─ Recursive descent through nested objects

4. ensureLibraryLoaded(info, moduleIndex)
   ├─ If same module already loaded → return true (reuse)
   ├─ If different module running → stopWallpaper()
   ├─ m_library.unload()                 // FreeLibrary old DLL
   ├─ m_library.load(dll_path, error)    // LoadLibraryEx new DLL
   │   ├─ LoadLibraryExW(path, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | ...)
   │   └─ GetProcAddress(m_lib, "createModule") → m_createFn
   └─ m_currentModuleId = moduleIndex

5. if (m_running) stopWallpaper()

6. startWorker(info)
   ├─ m_originalWallpaper = GetCurrentWallpaperPath()  // Save for restore
   ├─ m_running = true
   └─ m_worker = std::thread([this, info]() {
        ├─ Read settings.json → json object
        ├─ m_module.reset(m_library.createFn()(settings.dump().c_str()))
        │   └─ DLL's createModule():
        │       ├─ Parse JSON
        │       ├─ Settings::Instance(json).loadFromJson(json)
        │       ├─ new Application(json)
        │       │   ├─ settings_(Settings::Instance(json))  // reference
        │       │   ├─ window_(settings_.MSAA)
        │       │   │   ├─ glfwWindowHint(...) × N
        │       │   │   ├─ glfwCreateWindow(desktop_w, desktop_h, ...)
        │       │   │   ├─ glfwMakeContextCurrent(window)
        │       │   │   └─ gladLoadGLLoader(...)            // OpenGL loader
        │       │   ├─ starSystem_(settings_, bounds_rect)
        │       │   ├─ renderer_(settings_, width, height)
        │       │   │   ├─ compileShaders(vert, frag) → GLProgram
        │       │   │   ├─ glGenVertexArrays → VertexArray
        │       │   │   ├─ glGenBuffers → ArrayBuffer
        │       │   │   ├─ glGenTextures → GLTexture
        │       │   │   └─ glGenFramebuffers → FrameBuffer
        │       │   ├─ initWindow() → glfwGetCursorPos
        │       │   └─ initOpenGL() → vsync, tick function
        │       └─ return pointer
        │
        ├─ if (!m_module) throw "factory returned null"
        ├─ SetWindowLongPtrW(hwnd, GWL_STYLE, WS_VISIBLE)  // Un-hide window
        ├─ AttachWindowToDesktop(hwnd)                      // Parent to WorkerW
        └─ m_module->run()
            └─ mainLoop()
                └─ while(running) { update; render; swap; poll; tick; }
      })
```

---

## Module Shutdown Sequence

### Normal Shutdown (User clicks "Stop" from main thread)

```
User clicks "Stop" in QML toolbar
  → backend.stopWallpaper()

1. Guard: if (!m_running) return

2. if (m_module) {
     ├─ IsWindow(m_module->hwnd())          // Validate HWND still exists
     ├─ SetWindowLongPtrW(hwnd, GWL_STYLE, ~WS_VISIBLE)  // Hide window
     ├─ DetachWindowFromDesktop(hwnd)       // Remove from WorkerW
     └─ m_module->stop()                    // Set running = false (atomic)
   }

3. if (m_worker.joinable() && this_thread != m_worker) {
     m_worker.join()                        // Block until run() returns
   }

4. m_running = false

5. m_module.reset()                         // ← ⚠️ CRASH LOCATION
   └─ delete IWallpaperModule*
      └─ ~Application()                     // Compiler-generated
         ├─ atomic<bool> running            // Trivial
         ├─ ... timing/input members        // Trivial
         ├─ ~Renderer()                     // glDelete* (NO GL CONTEXT!)
         ├─ ~vector<Vertex>()               // Trivial
         ├─ ~StarSystem()                   // Trivial
         ├─ ~Window()                       // glfwDestroyWindow() (WRONG THREAD!)
         └─ ~GlfwContext()                  // glfwTerminate() (WRONG THREAD!)

6. restoreWallpaper()                       // SetWallpaper(m_originalWallpaper)
```

> **THE CRASH:** Step 5 destroys GLFW/OpenGL resources on the **main thread**, but they were created on the **worker thread**. GLFW requires `glfwDestroyWindow()` and `glfwTerminate()` to be called from the same thread that called `glfwInit()`. The OpenGL context is only valid on the worker thread.

### Correct Shutdown (after fix)

```
Worker thread lambda:
  m_module->run()          // Returns when stop() sets running=false
  m_module.reset()         // ← FIX: Destroy on WORKER thread
     └─ All GLFW/GL teardown runs on correct thread ✓

Main thread (stopWallpaper):
  m_module->stop()         // Signal exit
  m_worker.join()          // Wait for worker (which now includes destruction)
  // m_module.reset()      // No-op — already null from worker thread
  restoreWallpaper()
```

### Self-Stop (called from worker thread, e.g. via invokeMethod)

```
Worker thread executes stopWallpaper():
  m_module->stop()          // Sets running=false (same thread)
  m_worker.detach()         // Can't join self → detach
  m_running = false
  m_module.reset()          // Destroys module on worker thread ✓
                            // BUT: we may still be inside m_module's call stack!
  restoreWallpaper()
```

> **Risk:** If `stopWallpaper()` is called from code that is itself called by the module (re-entrant), `m_module.reset()` destroys the module while still inside its call stack. This is a secondary crash risk. Most likely, this path isn't hit in practice (Qt signal from QML dispatches on main thread).

---

## Application Shutdown

```
QApplication::quit() triggered (tray menu or window close)
  → QCoreApplication::aboutToQuit signal
  → WallpaperController::stopWallpaper()    // Stop any running module
  → ~WallpaperController()
     ├─ stopWallpaper() again (idempotent via m_running guard)
     └─ wallpaper::desktop::shutdownWallpaperMethod()
  → ~ModuleLibrary()
     └─ FreeLibrary(m_lib)                  // Unload module DLL
  → ~QQmlApplicationEngine                 // Destroy QML
  → ~QApplication                          // Qt cleanup
```

---

## Crash-Prone Lifecycle Points

| Phase | Risk | Severity |
|---|---|---|
| Module creation | `Settings::Instance()` throws → `std::terminate()` | Medium |
| Module execution | `running` flag checked once per frame — may delay stop by ~16ms | Low |
| `m_module->stop()` | Called cross-thread — safe (atomic store) | None |
| `m_module->hwnd()` | Called cross-thread — safe (returns const pointer) | None |
| `m_worker.join()` | Blocks main thread until `run()` returns — correct | None |
| **`m_module.reset()`** | **Cross-thread GLFW/GL destruction — CRASH** | **Critical** |
| DLL unload | `FreeLibrary` happens after `m_module.reset()` — safe | None |
| Wallpaper restore | Win32 `SetWallpaper` — independent of module state | None |

---

## Thread Lifecycle Rules

1. **Worker thread is the GLFW thread.** All GLFW init, window creation, context management, rendering, and teardown happen here.
2. **Main thread never touches GL.** The Qt main thread only calls `m_module->stop()` (atomic store) and `m_module->hwnd()` (read-only pointer return).
3. **`stop()` does not block.** It signals the worker thread to exit, then returns immediately.
4. **`join()` blocks the main thread** until the worker thread's `run()` returns.
5. **Module destruction must be on the worker thread** — either inside the thread lambda after `run()` returns, or by ensuring the GL context is released and re-acquired on the destroying thread.
6. **DLL unload must happen after module destruction** — the `ModuleLibrary::unload()` call in `ensureLibraryLoaded()` or `~ModuleLibrary()` correctly occurs after `m_module.reset()`.
