# Example Module: Star Simulator

A detailed walkthrough of the `star-simulator` module, representing the canonical module architecture used by all Animura wallpaper plugins.

**Source:** `E:/coding/C/live-wallpaper-modules/star-simulator/`

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                  IWallpaperModule                    │
│                    (Interface)                       │
├─────────────────────────────────────────────────────┤
│                  Application                        │
│  ┌──────────┐ ┌────────┐ ┌──────────────────────┐  │
│  │GlfwContext│ │Window  │ │     Renderer         │  │
│  │glfwInit   │ │GLFWwin │ │ VAO, VBO, shaders,   │  │
│  │glfwTerm   │ │dow+GL  │ │ FBO, draw calls      │  │
│  └──────────┘ └────────┘ └──────────────────────┘  │
│  ┌──────────┐ ┌─────────────────────────────────┐  │
│  │Settings& │ │        StarSystem               │  │
│  │(singleton│ │ std::vector<Star> + update loop │  │
│  │ ref)     │ │                                  │  │
│  └──────────┘ └─────────────────────────────────┘  │
│  mainLoop(): while(running) { update; render; }     │
└─────────────────────────────────────────────────────┘
```

---

## File Map

| File | Role |
|---|---|
| `src/module.cpp` | DLL entry point — `createModule()` factory |
| `headers/application.hpp` | `Application` class — implements `IWallpaperModule` |
| `src/application.cpp` | Constructor, `run()`, `stop()`, `mainLoop()` |
| `headers/raii.hpp` / `src/raii.cpp` | RAII wrappers: `GlfwContext`, `Window`, `VertexArray`, `ArrayBuffer`, `GLTexture`, `FrameBuffer`, `GLProgram` |
| `headers/renderer.hpp` / `src/renderer.cpp` | OpenGL rendering — shader setup, star/line drawing |
| `headers/settings.hpp` / `src/settings.cpp` | `Settings` singleton — parsed from JSON |
| `headers/star_system.hpp` / `src/star_system.cpp` | Star simulation — spawn, update, bounds |
| `headers/star.hpp` / `src/star.cpp` | Individual star entity |
| `headers/types.hpp` | `Vertex`, `Color`, `Rect` data types |
| `headers/shader_utils.hpp` / `src/shader_utils.cpp` | GLSL shader compilation helpers |
| `CMakeLists.txt` | Build configuration — shared library DLL |

---

## DLL Entry Point: `src/module.cpp`

```cpp
extern "C" __declspec(dllexport)
IWallpaperModule* createModule(const char* settingsJson) {
    auto settings = nlohmann::json::parse(settingsJson);
    Settings::Instance(settings).loadFromJson(settings);
    return new Application(settings);
}
```

### What happens:
1. Parse the JSON settings string
2. Get/create the `Settings` singleton and populate it
3. Allocate and return a new `Application`

The returned pointer is owned by the host's `unique_ptr<IWallpaperModule>`.

---

## Application Class: `headers/application.hpp`

```cpp
class Application : public IWallpaperModule {
public:
    Application(const nlohmann::json& settingsJson);
    void run() override;
    void stop() override;
    HWND hwnd() const override;

private:
    GlfwContext   glfwContext_{};      // glfwInit/glfwTerminate
    Settings&     settings_;           // Reference to singleton
    Window        window_;             // GLFW window (RAII)
    StarSystem    starSystem_;         // Star simulation
    std::vector<Vertex> vertices_;     // Per-frame geometry buffer
    Renderer      renderer_;           // OpenGL draw calls
    std::atomic<bool> running{false};  // Render loop control
    // ... timing, input state ...
};
```

### Member Destruction Order (compiler-generated destructor)

Declared first → destroyed last. So:

```
Creation order (constructor):
  GlfwContext → Settings& → Window → StarSystem → vertices → Renderer → ... → running

Destruction order (destructor):
  running → ... → Renderer → vertices → StarSystem → Window → Settings& → GlfwContext
```

This is **correct**: OpenGL objects (`Renderer`) are destroyed while the context (`Window`) is still alive, and `glfwTerminate()` (`GlfwContext`) runs after `glfwDestroyWindow()` (`Window`).

---

## Settings Singleton: `headers/settings.hpp`

```cpp
class Settings {
public:
    static Settings& Instance(const nlohmann::json& j) noexcept;
    void loadFromJson(const nlohmann::json& j);
    // ... config fields ...
};
```

Implementation:
```cpp
Settings& Settings::Instance(const nlohmann::json& j) noexcept {
    static Settings instance(j);   // Meyer's singleton
    return instance;
}
```

**Important:** Marked `noexcept` but the constructor calls `loadFromJson()` which can throw (e.g., missing JSON key). If the first call throws, `std::terminate()` is invoked. This is a latent bug.

**Lifetime:** The singleton lives in the DLL's static storage. It survives for the DLL's entire loaded lifetime, so `Application::settings_` (a reference) is always valid.

---

## Render Loop: `mainLoop()`

```cpp
void Application::mainLoop() {
    auto previous = std::chrono::high_resolution_clock::now();
    while (running) {
        const auto now = std::chrono::high_resolution_clock::now();
        const GameTickDuration dt = now - previous;
        previous = now;

        glfwGetCursorPos(window_.get(), &mouseX_, &mouseY_);
        starSystem_.update(dt, mouseX_, height_ - mouseY_);
        renderer_.updateFrameGeometry(starSystem_, vertices_, mouseX_, mouseY_);
        renderer_.uploadVertices(vertices_);
        renderer_.render();
        glfwSwapBuffers(window_.get());
        glfwPollEvents();
        tickFunc_(dt, stepInterval_, fractionalTime_);
    }
}
```

### Frame sequence:
1. **Delta time:** Compute frame duration
2. **Input:** Poll mouse position via GLFW
3. **Simulation:** Update star positions in `StarSystem`
4. **Geometry:** Build vertex buffer (stars + mouse-proximity lines)
5. **Upload:** Send vertex data to GPU (`glBufferData` with `GL_DYNAMIC_DRAW`)
6. **Render:** Draw background gradient quad → draw stars and lines
7. **Present:** `glfwSwapBuffers` (vsync may block here)
8. **Poll:** `glfwPollEvents` processes window events
9. **Tick:** Frame pacing — either vsync passthrough or `sleep_for` to cap FPS

### Tick Functions
- **Vsync mode:** `vsyncTick` — no-op, `glfwSwapBuffers` already throttles
- **Fixed FPS mode:** `sleepTick` — computes remaining time in the frame budget, sleeps with fractional carryover

---

## RAII Classes: `headers/raii.hpp`

### `GlfwContext`
```cpp
struct GlfwContext {
    GlfwContext();                    // calls glfwInit()
    ~GlfwContext() noexcept;          // calls glfwTerminate()
};
```
Non-copyable, movable.

### `Window`
```cpp
class Window {
    GlfwWindowPtr window_;  // unique_ptr<GLFWwindow, custom deleter>
public:
    explicit Window(int msaa = 0);   // creates window, makes GL context current
    GLFWwindow* get() const;         // raw GLFWwindow*
    HWND hwnd() const;              // native Win32 handle via glfwGetWin32Window
    float width()/height() const;   // logical dimensions (pixels)
};
```
Window hints: OpenGL 3.3 Core, non-resizable, invisible (until desktop-attached), undecorated, transparent framebuffer.

### `VertexArray` / `ArrayBuffer` / `GLTexture` / `FrameBuffer` / `GLProgram`
Standard OpenGL object wrappers. Each:
- Generates the GL object in the constructor
- Deletes it in the destructor (`glDelete*`)
- Non-copyable, movable (move transfers ownership, resets source to 0)

### `Renderer`
Holds the shader programs, VAO, VBO, and framebuffer. Constructed with `Settings&` to read star parameters at init time. Key methods:
- `rebuildStaticData()` — preallocates vertex buffer based on star count
- `updateFrameGeometry()` — rebuilds per-frame vertex data
- `uploadVertices()` — sends vertex data to GPU
- `render()` — draws the background FBO texture (fullscreen quad) then the star/line geometry

---

## Known Risk Areas

### 1. Cross-Thread Destruction (HOST BUG)
The host destroys `Application` from the main thread, but `glfwTerminate()` and `glDelete*` require the worker thread's GL context. **This is the primary crash cause.**

### 2. Settings noexcept Violation
`Settings::Instance()` is `noexcept` but can throw on first call (bad JSON). This causes `std::terminate()`.

### 3. No Explicit Destructor
`Application` relies on the compiler-generated destructor. This is correct for the member order, but there's no explicit guarantee or comment documenting why the order matters.

### 4. FrameBuffer Lifetime vs Renderer
`FrameBuffer` (contained in `Renderer`) holds a `GLTexture` member. Both are destroyed by `~Renderer()`. The `FrameBuffer` destructor calls `glDeleteFramebuffers` — this requires a current GL context (satisfied because `Window` destructor runs after `Renderer`).

### 5. glfwPollEvents Blocking
On some systems, `glfwPollEvents()` can block briefly waiting for window messages. This doesn't cause crashes but adds latency to `stop()` response.

---

## Build Configuration

```cmake
# Key settings
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # /MT static CRT
set(CMAKE_CXX_STANDARD 23)
add_library(module SHARED ${APP_SOURCES})          # Builds as DLL
set_target_properties(module PROPERTIES PREFIX "" SUFFIX ".dll")
target_link_libraries(module PRIVATE OpenGL::GL glfw)
```

Shaders are embedded at build time via `cmake/embed_shaders.cmake` → generates `shaders.hpp` containing GLSL source as string literals.
