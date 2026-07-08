#pragma once

#include <Windows.h>

/**
 * @brief ABI-stable interface contract between the Animura host and wallpaper
 *        module DLLs.
 *
 * Every wallpaper module must implement this exact interface. The header must
 * be **byte-identical** between the host copy and all module copies to
 * guarantee vtable compatibility across the DLL boundary.
 *
 * ## Lifecycle Contract
 * ```
 * createModule(json)  →  run() [BLOCKS]  →  stop() [non-blocking]  →  ~IWallpaperModule()
 *    (worker thread)      (worker thread)      (main thread)              (worker thread)
 * ```
 *
 * - `run()` starts the render loop and blocks until `stop()` signals exit.
 * - `stop()` only sets an atomic flag; it does NOT wait, join threads, or
 *   touch rendering library state.
 * - The destructor must execute on the **same thread** that created the
 *   module, because many rendering libraries require thread-affine teardown
 *   (e.g., GLFW's `glfwDestroyWindow()`/`glfwTerminate()` must be called
 *   from the thread that called `glfwInit()`).
 *
 * ## ABI Stability Requirements
 * - Both host and module must compile with **static CRT (/MT)** — mixing
 *   CRT types causes heap corruption when the host deletes objects
 *   allocated by the module.
 * - Virtual method order must never change.
 * - The module exports a `createModule` factory function (unmangled via
 *   `extern "C"`) that returns a heap-allocated instance — ownership
 *   transfers to the caller.
 *
 * @see WallpaperController for the host-side lifecycle manager.
 * @see ModuleLibrary for DLL loading and factory resolution.
 */
class IWallpaperModule {
public:
    /**
     * @brief Virtual destructor — required for correct polymorphic deletion
     *        across the DLL boundary.
     *
     * The compiler-generated vtable entry ensures that `delete` on a base
     * pointer dispatches to the most-derived destructor chain.
     *
     * @warning Must execute on the **worker thread** — the same thread that
     *          created the module. Many rendering libraries require teardown
     *          on the initialization thread (e.g., GLFW's
     *          `glfwDestroyWindow()`/`glfwTerminate()`).
     */
    virtual ~IWallpaperModule() = default;

    /**
     * @brief Starts the module's render loop. **Blocking call.**
     *
     * Called from the worker thread after the module has been created and
     * its window has been attached to the desktop WorkerW layer.
     *
     * The implementation enters a frame loop:
     * ```
     * running = true;
     * while (running) {
     *     update(dt);      // advance simulation
     *     render();        // upload geometry, issue draw calls
     *     present();       // swap buffers / present frame
     *     pollEvents();    // process window events
     * }
     * ```
     *
     * Returns only after `stop()` has set the atomic `running` flag to
     * false and the current frame iteration completes.
     *
     * @note All rendering calls within `run()` execute on the caller's
     *       thread. Modules using libraries with thread affinity (e.g.,
     *       OpenGL, Direct3D, Vulkan) must ensure `run()` is called from
     *       the same thread that performed initialization.
     *
     * @see stop() for the non-blocking exit signal.
     */
    virtual void run() = 0;

    /**
     * @brief Signals the render loop to exit. **Non-blocking.**
     *
     * Called from the Qt main thread (via WallpaperController). The
     * implementation must only set an atomic flag:
     * ```
     * running.store(false);
     * ```
     *
     * It must NOT:
     * - Block or join threads.
     * - Call any rendering library function.
     * - Wait for `run()` to return.
     *
     * The host calls `m_worker.join()` after `stop()` to synchronize with
     * the worker thread exit.
     *
     * @see run() for the blocking render loop that observes this flag.
     */
    virtual void stop() = 0;

    /**
     * @brief Returns the native Win32 window handle of the module's window.
     *
     * Used by the host to:
     * - Attach the window to the desktop WorkerW layer (behind icons).
     * - Detach the window on wallpaper stop.
     * - Show/hide the window during lifecycle transitions.
     *
     * @return HWND of the module's window. For GLFW-based modules this is
     *         obtained via `glfwGetWin32Window()`; other libraries provide
     *         their own native handle accessors.
     *
     * @note Thread-safe — returns a const handle, no state mutation.
     *       Safe to call from the main thread while the render loop runs
     *       on the worker thread.
     */
    virtual HWND hwnd() const = 0;
};
