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
   ├─ WallpaperController wallpaperController
   │   └─ ModuleCatalog("modules") → scans ./modules/ directory
   │       └─ Reads each subdirectory's module.json
   │       └─ Validates all required files exist
   │       └─ Populates m_modules vector
   │
   ├─ WebView2Host webViewHost(&wallpaperController)
   │   ├─ Creates QWidget window
   │   ├─ initWebView2()
   │   │   ├─ CreateCoreWebView2EnvironmentWithOptions
   │   │   ├─ Create NativeBridge(&wallpaperController)
   │   │   ├─ Register bridge as "nativeBridge" host object
   │   │   ├─ Map "animura.modules" virtual host → <exe_dir>\modules\
   │   │   └─ navigateToApp()
   │   │       ├─ Dev mode: Navigate("http://localhost:5173")
   │   │       └─ Production: Map "animura.app" → frontend/dist\
   │   │                      Navigate("https://animura.app/index.html")
   │   └─ QWidget* mainWindow = webViewHost.widget()
   │
   ├─ QObject::connect: runningModuleChanged → PostWebMessageAsJson
   ├─ QLocalServer → listens for "show" from other instances
   ├─ connect(aboutToQuit → stopWallpaper)
   ├─ setupTrayIcon()
   │
   └─ app.exec() → Qt event loop
```

## Module Loading Sequence

```
User clicks "Start" in SettingsPanel (React)
  → NativeBridge.StartWallpaper(moduleIndex)    [COM call]
  → Qt main thread: WallpaperController::startWallpaper(moduleIndex)

1. validateIndex(moduleIndex)
   └─ Bounds check on m_catalog.modules()

2. validateModuleFiles(info)
   └─ std::filesystem::exists(dll_path, schema_path, settings_path)

3. validateModuleSettings(info)
   └─ Read schema.json + settings.json → parse JSON
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
   ├─ emit runningModuleChanged()         // → PostWebMessageAsJson → React
   └─ m_worker = std::thread([this, info]() {
        ├─ Read settings.json → json object
        ├─ m_module.reset(m_library.createFn()(settings.dump().c_str()))
        │   └─ DLL's createModule():
        │       ├─ Parse JSON
        │       ├─ Settings::Instance(json).loadFromJson(json)
        │       ├─ new Application(json)
        │       │   ├─ GlfwContext → glfwInit()
        │       │   ├─ Window → glfwCreateWindow(), glfwMakeContextCurrent()
        │       │   ├─ gladLoadGLLoader()
        │       │   ├─ Renderer → compile shaders, VAO, VBO, FBO
        │       │   └─ StarSystem, etc.
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

## Module Shutdown Sequence

### Normal Shutdown (User clicks "Stop" from React)

```
User clicks "Stop" in header or SettingsPanel
  → NativeBridge.StopWallpaper()             [COM call]
  → Qt main thread: WallpaperController::stopWallpaper()

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
   emit runningModuleChanged()              // → PostWebMessageAsJson → React

5. m_module.reset()                         // ← ⚠️ CRASH LOCATION
   └─ delete IWallpaperModule*
      └─ ~Application()                     // Compiler-generated
         ├─ ~Renderer()                     // glDelete* (NO GL CONTEXT!)
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

---

## Application Shutdown

```
QApplication::quit() triggered (tray menu)
  → QCoreApplication::aboutToQuit signal
  → WallpaperController::stopWallpaper()    // Stop any running module
  → ~WallpaperController()
     ├─ stopWallpaper() again (idempotent via m_running guard)
     └─ wallpaper::desktop::shutdownWallpaperMethod()
  → ~WebView2Host()
     ├─ m_webViewController->Close(), Release()
     ├─ m_webView->Release()
     ├─ m_env->Release()
     └─ m_bridge->Release()
  → ~ModuleLibrary()
     └─ FreeLibrary(m_lib)                  // Unload module DLL
  → ~QApplication                          // Qt cleanup
```

---

## Frontend → C++ Communication Flow

```
1. React calls bridge function (e.g., bridge.getModulesList())
     → window.chrome.webview.hostObjects.nativeBridge.GetModulesList()

2. WebView2 COM proxy:
     → Calls NativeBridge::GetIDsOfNames("GetModulesList") → returns DISPID 1
     → Calls NativeBridge::Invoke(DISPID 1, ...)

3. NativeBridge::Invoke (on WebView2 thread):
     → QMetaObject::invokeMethod(m_controller, [&]() { ... },
         Qt::BlockingQueuedConnection)
     → Blocks WebView2 thread until main thread completes

4. WallpaperController (on Qt main thread):
     → Executes the requested method
     → If startWallpaper/stopWallpaper: emit runningModuleChanged()
     → Signal handler in main.cpp: PostWebMessageAsJson to JS

5. NativeBridge::Invoke returns result as BSTR:
     → WebView2 converts BSTR → JS string
     → JS bridge wrapper parses/coerces types (e.g., Number(result))

6. React state updates:
     → setRunningModuleId(), setModules(), etc.
     → Components re-render with new data
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
| NativeBridge marshaling | Cross-thread with BQ connection — correct | None |

---

## Thread Lifecycle Rules

1. **Worker thread is the GLFW thread.** All GLFW init, window creation, context management, rendering, and teardown happen here.
2. **Main thread never touches GL.** The Qt main thread only calls `m_module->stop()` (atomic store) and `m_module->hwnd()` (read-only pointer return).
3. **`stop()` does not block.** It signals the worker thread to exit, then returns immediately.
4. **`join()` blocks the main thread** until the worker thread's `run()` returns.
5. **Module destruction must be on the worker thread** — either inside the thread lambda after `run()` returns, or by ensuring the GL context is released and re-acquired on the destroying thread.
6. **DLL unload must happen after module destruction** — `ModuleLibrary::unload()` correctly occurs after `m_module.reset()`.
7. **NativeBridge marshals to main thread** — WebView2 threads never touch C++ state directly.
