#pragma once

#include <filesystem>

/**
 * @brief Provides standardized filesystem paths for the Animura application.
 *
 * Paths resolves the user's Local AppData directory via the Windows
 * `SHGetKnownFolderPath` API and maintains the Animura subdirectory
 * underneath it. All application-scoped persistent files (e.g., the
 * settings file) are stored under this directory.
 *
 * ## Usage
 * Call `Paths::init()` once at startup to create the directory structure
 * and cache the resolved paths. Subsequent calls to `appData()` and
 * `settings()` return the cached values without additional system calls.
 *
 * If a path accessor is called before `init()`, it calls `init()` lazily.
 *
 * ## Directory Layout
 * ```
 * %LOCALAPPDATA%\Animura\
 * └── settings.json    ← Application-scoped settings
 * ```
 *
 * @see AppSettings for the class that reads and writes the settings file.
 */
class Paths {
public:
    /**
     * @brief Initializes path resolution and creates the Animura directory
     *        under Local AppData if it does not already exist.
     *
     * Must be called once before accessing any paths. Safe to call
     * multiple times — subsequent calls are no-ops.
     */
    static void init();

    /**
     * @brief Returns the absolute path to the Animura application data
     *        directory.
     *
     * Resolves to `%LOCALAPPDATA%/Animura/`. The directory is guaranteed
     * to exist after the first call.
     *
     * @return Absolute filesystem path to the Animura data directory.
     */
    static std::filesystem::path appData();

    /**
     * @brief Returns the absolute path to the application settings file.
     *
     * Resolves to `%LOCALAPPDATA%/Animura/settings.json`.
     *
     * @return Absolute filesystem path to `settings.json`.
     */
    static std::filesystem::path settings();

private:
    /** Whether `init()` has been called successfully. */
    static bool initialized;

    /** Cached path to the Animura application data directory. */
    static std::filesystem::path appData_;

    /** Cached path to the settings JSON file. */
    static std::filesystem::path settings_;
};
