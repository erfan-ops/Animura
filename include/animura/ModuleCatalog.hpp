#pragma once
#include "ModuleInfo.hpp"
#include <vector>

/**
 * @brief Discovers available wallpaper modules by scanning the
 *        `./modules/` directory for subdirectories containing valid
 *        `module.json` manifests.
 *
 * ModuleCatalog is constructed once at startup by WallpaperController.
 * It scans one level deep, parses each `module.json`, validates that
 * all required fields are present and all referenced files exist, and
 * builds a vector of ModuleInfo structs.
 *
 * ## Required module.json Fields
 * - `name`    — Display name.
 * - `version` — Semantic version string.
 * - `entry`   — DLL filename (e.g., `"module.dll"`).
 * - `schema`  — Schema filename (e.g., `"schema.json"`).
 * - `settings`— Settings filename (e.g., `"settings.json"`).
 * - `preview` — Preview image filename (e.g., `"preview.png"`).
 *
 * ## Error Behavior
 * - Individual module parse failures are logged to `std::cout` but do not
 *   prevent other modules from being discovered.
 * - If zero valid modules are found, the constructor throws
 *   `std::runtime_error("No modules were found")` — the application cannot
 *   function without at least one module.
 * - If the `modules/` directory itself is missing, throws
 *   `std::runtime_error("Modules folder not found!")`.
 *
 * @see ModuleInfo for the per-module metadata struct.
 */
class ModuleCatalog {
public:
    /**
     * @brief Scans the given root directory for wallpaper modules.
     *
     * @param root Path to the modules directory. Defaults to `"modules"`,
     *             resolved relative to the executable's working directory.
     *
     * @throws std::runtime_error if the directory is missing or contains
     *         zero valid modules.
     */
    explicit ModuleCatalog(const std::filesystem::path& root = "modules");

    /**
     * @brief Returns the list of discovered modules.
     *
     * Used by WallpaperController to build the module grid data and
     * look up modules by index for start/stop operations.
     *
     * @return Const reference to the internal vector of ModuleInfo.
     */
    const std::vector<ModuleInfo>& modules() const { return m_modules; }

private:
    /** Validated module metadata, populated by scanModules(). */
    std::vector<ModuleInfo> m_modules;

    /**
     * @brief Recursively scans a directory for module subdirectories.
     *
     * For each subdirectory containing a `module.json`:
     * 1. Parses the JSON via JsonUtils.
     * 2. Validates all 6 required fields are present.
     * 3. Verifies all referenced files exist on disk.
     * 4. On success, appends a ModuleInfo to `m_modules`.
     *
     * @param root The directory to scan (typically `"modules"`).
     */
    void scanModules(const std::filesystem::path& root);
};
