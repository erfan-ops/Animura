#pragma once

#include <QObject>
#include <QJsonObject>

#include <thread>
#include <atomic>
#include <memory>

#include "ModuleCatalog.hpp"
#include "ModuleLibrary.hpp"
#include "IWallpaperModule.hpp"

/**
 * @brief Central orchestrator for wallpaper module lifecycle.
 *
 * WallpaperController is the heart of the Animura host. It owns:
 * - Module discovery (via ModuleCatalog).
 * - DLL loading/unloading (via ModuleLibrary).
 * - Worker thread management for running wallpaper render loops.
 * - Desktop wallpaper save/restore.
 * - Settings file I/O and validation.
 *
 * All Q_INVOKABLE methods are called from the React frontend through the
 * NativeBridge COM object, which marshals calls to the Qt main thread.
 * WallpaperController itself lives on the Qt main thread; it spawns a
 * worker thread for module execution because many rendering libraries
 * require thread affinity.
 *
 * ## Threading Model
 * - **Main thread**: All public methods, `stop()`, `hwnd()`.
 * - **Worker thread**: `createModule()`, `run()`, `~IWallpaperModule()`,
 *   and all rendering library calls.
 *
 * @see NativeBridge for the COM bridge that exposes this controller to JS.
 * @see IWallpaperModule for the module ABI contract.
 */
class WallpaperController : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs the controller and initializes desktop wallpaper
     *        integration.
     *
     * Scans the `./modules/` directory via ModuleCatalog and calls
     * `wallpaper::desktop::initSetWallpaperMethod()` to prepare the
     * wallpaper save/restore backend.
     *
     * @param parent Qt parent object (default nullptr).
     *
     * @throws std::runtime_error if no modules are discovered in
     *         `./modules/`.
     */
    explicit WallpaperController(QObject* parent = nullptr);

    /**
     * @brief Destroys the controller and shuts down wallpaper integration.
     *
     * Calls `stopWallpaper()` to stop any running module, then calls
     * `wallpaper::desktop::shutdownWallpaperMethod()` to clean up the
     * wallpaper backend.
     */
    ~WallpaperController();

    /**
     * @brief Returns the list of discovered modules for display in the
     *        React module grid.
     *
     * Each entry contains `name`, `version`, `previewPath` (a virtual-host
     * URL resolved by WebView2), and `id` (the module index).
     *
     * @return QVariantList of QVariantMap objects, one per module.
     */
    Q_INVOKABLE QVariantList getModulesList() const;

    /**
     * @brief Returns the index of the currently running module, or -1 if
     *        no module is active.
     *
     * @return int Module index (0-based) if running, -1 otherwise.
     */
    Q_INVOKABLE int getRunningModuleId() const;

    /**
     * @brief Starts a wallpaper module by its index in the catalog.
     *
     * Full sequence:
     * 1. Validate the index is in bounds.
     * 2. Verify the module's DLL, schema, and settings files exist on disk.
     * 3. Validate settings JSON against the module's schema.
     * 4. Load the module DLL (unloading any previously loaded DLL).
     * 5. Stop any currently running module.
     * 6. Spawn a worker thread that creates the module, attaches its window
     *    to the desktop WorkerW, and enters the blocking render loop.
     *
     * Emits `runningModuleChanged()` on start.
     *
     * @param moduleIndex Zero-based index into the module catalog.
     */
    Q_INVOKABLE void startWallpaper(int moduleIndex);

    /**
     * @brief Stops the currently running wallpaper module and restores the
     *        original Windows wallpaper.
     *
     * Full sequence:
     * 1. Guard: if nothing is running, return immediately.
     * 2. Detach the module window from the desktop WorkerW and hide it.
     * 3. Signal the module to stop via `m_module->stop()` (atomic flag).
     * 4. Join the worker thread (blocks until the render loop exits).
     *    - The worker lambda destroys the module on the worker thread
     *      after `run()` returns, ensuring rendering library teardown
     *      happens on the correct thread.
     * 5. Set `m_running = false` and emit `runningModuleChanged()`.
     * 6. Restore the saved original Windows wallpaper image.
     *
     * Emits `runningModuleChanged()` on stop.
     *
     * @note Idempotent — safe to call when no module is running.
     * @warning If called from the worker thread itself, the worker is
     *          detached instead of joined to avoid self-join deadlock.
     */
    Q_INVOKABLE void stopWallpaper();

    /**
     * @brief Loads the settings schema and current values for the
     *        specified module's settings panel.
     *
     * Reads `schema.json` and `settings.json` from the module's directory
     * and returns them as a JSON object with `schema` and `settings` keys.
     * The React `SettingsControl` component recursively generates form
     * controls from the schema.
     *
     * @param moduleIndex Index of the module to load settings for.
     * @return QJsonObject with keys `"schema"` and `"settings"`, or an
     *         empty object if the index is invalid.
     */
    Q_INVOKABLE QJsonObject loadSettingsUI(int moduleIndex);

    /**
     * @brief Writes the provided settings JSON to the module's
     *        `settings.json` file atomically.
     *
     * Uses `QSaveFile` for atomic writes — data is written to a temporary
     * file, then renamed over the target on successful commit. This
     * prevents file corruption if the application crashes mid-write.
     *
     * Settings are validated against the module's schema at start time,
     * not at write time.
     *
     * @param moduleIndex Index of the module whose settings to update.
     * @param settings    New settings values as a QJsonObject.
     */
    Q_INVOKABLE void applySettings(int moduleIndex, QJsonObject settings);

    /**
     * @brief Installs a wallpaper module from a ZIP package at the given path.
     *
     * The file dialog is opened by NativeBridge (which has the QWidget parent
     * for proper window stacking). This method receives the selected path and
     * performs the installation.
     *
     * ## Installation Workflow
     * 1. Opens the ZIP — validates that `module.json` exists at the root.
     * 2. Extracts `module.json` to a temp location and parses the `id`.
     * 3. Rejects if the `id` is missing, empty, or already in the catalog.
     * 4. Extracts the full ZIP to `modules/<id>/`.
     * 5. Constructs a ModuleInfo and adds it to the catalog.
     * 6. Emits `modulesChanged()` so the frontend refreshes.
     *
     * If any step fails, cleans up any partially extracted files and
     * returns the error message. On success, returns an empty string.
     *
     * @param zipPath Full path to the `.zip` package.
     * @return Empty string on success, or a human-readable error message.
     */
    Q_INVOKABLE QString installModuleFromPath(const QString& zipPath);

signals:
    /**
     * @brief Emitted when the running state changes (module started or
     *        stopped).
     *
     * Connected in main.cpp to forward state changes to the React frontend
     * via WebView2's `PostWebMessageAsJson`.
     */
    void runningModuleChanged();

    /**
     * @brief Emitted when the module catalog changes (e.g., a new module
     *        was installed at runtime).
     *
     * Connected in main.cpp to forward catalog updates to the React
     * frontend so the module grid refreshes automatically.
     */
    void modulesChanged();

private:
    /** Discovered module list from scanning `./modules/`. */
    ModuleCatalog m_catalog;

    /** Currently loaded DLL handle and factory function pointer. */
    ModuleLibrary m_library;

    /**
     * Active module instance. Owned by unique_ptr; the worker thread
     * creates it and destroys it inside the worker lambda after
     * `run()` returns, ensuring teardown on the correct thread.
     */
    std::unique_ptr<IWallpaperModule> m_module;

    /**
     * Worker thread that runs the module's render loop.
     * Spawned by `startWorker()`, joined by `stopWallpaper()`.
     */
    std::thread m_worker;

    /**
     * Whether a module is currently active. Atomic because it is written
     * on the main thread and read by the worker thread during `getRunningModuleId()`.
     */
    std::atomic_bool m_running{ false };

    /**
     * Index of the currently loaded module in the catalog.
     * -1 means no module is loaded.
     */
    int m_currentModuleId{ -1 };

    /**
     * Path of the original Windows wallpaper image, saved before module
     * start so it can be restored on stop.
     */
    std::wstring m_originalWallpaper;

    /**
     * @brief Restores the original Windows desktop wallpaper.
     *
     * Called during `stopWallpaper()`. Uses the path saved in
     * `m_originalWallpaper`.
     */
    void restoreWallpaper() const;

    /**
     * @brief Validates that a module index is within bounds.
     * @return true if the index is valid.
     */
    bool validateIndex(int moduleIndex) const;

    /**
     * @brief Verifies that a module's DLL, schema, and settings files
     *        exist on disk.
     * @return true if all required files exist.
     */
    bool validateModuleFiles(const ModuleInfo& info) const;

    /**
     * @brief Validates a module's settings JSON against its schema.
     *
     * Reads both files, then delegates to
     * `SettingsSchemaValidator::validate()`. Validation failures are
     * logged via `qWarning()`.
     *
     * @return true if settings conform to the schema.
     */
    bool validateModuleSettings(const ModuleInfo& info) const;

    /**
     * @brief Ensures the correct module DLL is loaded.
     *
     * If the requested module is already loaded, returns immediately.
     * Otherwise stops any running wallpaper, unloads the current DLL,
     * and loads the new one via `ModuleLibrary::load()`.
     *
     * @return true if the library was loaded successfully.
     */
    bool ensureLibraryLoaded(const ModuleInfo& info, int moduleIndex);

    /**
     * @brief Spawns the worker thread that creates and runs the module.
     *
     * The worker thread lambda:
     * 1. Reads settings JSON from disk.
     * 2. Calls `createModule(json)` to instantiate the module.
     * 3. Attaches the module's window to the desktop WorkerW.
     * 4. Calls `m_module->run()` (blocking — enters the render loop).
     * 5. After `run()` returns: destroys `m_module` on the worker thread,
     *    ensuring rendering library teardown happens on the correct
     *    thread.
     *
     * The original wallpaper path is saved before spawning the thread so
     * it can be restored on stop.
     *
     * @param info ModuleInfo for the module to start.
     */
    void startWorker(const ModuleInfo& info);
};
