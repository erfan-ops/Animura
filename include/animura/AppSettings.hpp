#pragma once

#include <nlohmann/json.hpp>
#include <string>

/**
 * @brief Manages application-scoped persistent settings stored in the
 *        user's Local AppData directory.
 *
 * AppSettings is a singleton that owns the `settings.json` file under
 * `%LOCALAPPDATA%/Animura/`. It handles loading, validation, modification,
 * and saving of application preferences.
 *
 * ## Settings Schema
 * | Key                      | Type    | Description                                   |
 * |--------------------------|---------|-----------------------------------------------|
 * | `restoreLastWallpaper`   | boolean | Whether to auto-start the last wallpaper      |
 * | `lastUsedWallpaperID`    | string  | Module ID of the most recently started module |
 *
 * ## Validation
 * On load, every required key is checked for existence and correct type.
 * If the file is missing, malformed, or fails validation, it is recreated
 * from defaults — no partial repair is attempted.
 *
 * ## Usage
 * ```cpp
 * AppSettings::instance().initialize();
 * if (AppSettings::instance().restoreLastWallpaper()) { ... }
 * AppSettings::instance().setRestoreLastWallpaper(false);
 * ```
 *
 * The destructor is deleted — the singleton lives for the entire process
 * lifetime.
 *
 * @see Paths::settings() for the file path.
 */
class AppSettings {
public:
    /**
     * @brief Returns the global singleton instance.
     *
     * Uses Meyer's singleton pattern — the instance is created on first
     * call and lives until process exit.
     *
     * @return Reference to the singleton AppSettings instance.
     */
    static AppSettings& instance();

    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;

    /**
     * @brief Loads settings from disk or creates defaults.
     *
     * Reads `Paths::settings()`. If the file does not exist, is malformed,
     * or fails validation, a new file with default values is written.
     *
     * Must be called once after `Paths::init()`. Subsequent calls are
     * no-ops.
     */
    void initialize();

    // ── Getters ──

    /**
     * @brief Returns whether the application should restore the last
     *        running wallpaper on startup.
     *
     * @return true if auto-restore is enabled.
     */
    bool restoreLastWallpaper() const;

    /**
     * @brief Returns the module ID of the most recently started wallpaper.
     *
     * This value is maintained automatically by the application and is
     * never exposed to the user for direct editing.
     *
     * @return The module ID string (e.g., "delaunay-flow").
     */
    std::string lastUsedWallpaperID() const;

    // ── Setters ──

    /**
     * @brief Enables or disables automatic wallpaper restore on startup.
     *
     * The change is persisted to disk immediately.
     *
     * @param value true to enable auto-restore, false to disable.
     */
    void setRestoreLastWallpaper(bool value);

    /**
     * @brief Records the given module ID as the most recently used
     *        wallpaper.
     *
     * Called automatically by WallpaperController every time a module is
     * started successfully. The change is persisted to disk immediately.
     *
     * @param id The module ID (e.g., "delaunay-flow").
     */
    void setLastUsedWallpaperID(const std::string& id);

    /**
     * @brief Writes the current settings to disk atomically.
     *
     * Uses `QSaveFile` for atomic write — data is written to a temporary
     * file, then renamed over the target. This prevents corruption if the
     * application crashes mid-write.
     *
     * Called internally by setters; exposed for cases where a batch of
     * changes must be persisted together.
     */
    void save();

private:
    AppSettings() = default;
    ~AppSettings() = default;

    /**
     * @brief Reads and parses the settings file from disk.
     *
     * Uses `std::ifstream` with RAII — the file is closed automatically
     * when the stream goes out of scope.
     *
     * @return true if the file was opened and parsed successfully.
     */
    bool loadFromDisk();

    /**
     * @brief Validates the loaded JSON against the required schema.
     *
     * Checks:
     * - `restoreLastWallpaper` exists and is a boolean.
     * - `lastUsedWallpaperID` exists and is a string.
     *
     * @param j The parsed JSON object to validate.
     * @return true if all required keys exist with correct types.
     */
    static bool validate(const nlohmann::json& j);

    /**
     * @brief Creates the default settings JSON and writes it to disk.
     *
     * Called when the settings file does not exist, cannot be parsed,
     * or fails validation.
     */
    void createDefault();

    /** The in-memory settings data. */
    nlohmann::json m_data;

    /** Whether initialize() has completed successfully. */
    bool m_initialized{ false };
};
