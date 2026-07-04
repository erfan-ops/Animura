#pragma once
#include <string>
#include <filesystem>
#include <QString>

/**
 * @brief Metadata and file paths for a single discovered wallpaper module.
 *
 * Populated by ModuleCatalog when scanning the `./modules/` directory.
 * Each field corresponds to a key in the module's `module.json` manifest.
 * Computed accessors combine `basePath` with relative filenames to produce
 * absolute filesystem paths.
 *
 * The `previewPathQt()` method returns a WebView2 virtual-host URL instead
 * of a filesystem path, because the React frontend loads preview images
 * through the `animura.modules` virtual host mapping.
 *
 * @see ModuleCatalog for the discovery logic that populates this struct.
 */
struct ModuleInfo {
    /**
     * Absolute path to the module's subdirectory (e.g.,
     * `C:\...\modules\star-simulator\`).
     */
    std::filesystem::path basePath;

    /** Display name from `module.json` (e.g., "Star Simulator"). */
    std::string name;

    /** Semantic version string from `module.json` (e.g., "1.0.0"). */
    std::string version;

    /** Relative DLL filename from `module.json` (e.g., "module.dll"). */
    std::filesystem::path entryDll;

    /** Relative schema filename from `module.json` (e.g., "schema.json"). */
    std::filesystem::path schemaFile;

    /** Relative settings filename from `module.json` (e.g., "settings.json"). */
    std::filesystem::path settingsFile;

    /** Relative preview image filename from `module.json` (e.g., "preview.png"). */
    std::filesystem::path previewFile;

    /** @return Absolute path to the module DLL: `basePath / entryDll`. */
    std::filesystem::path entryPath() const { return basePath / entryDll; }

    /** @return Absolute path to the JSON schema file: `basePath / schemaFile`. */
    std::filesystem::path schemaPath() const { return basePath / schemaFile; }

    /** @return Absolute path to the settings JSON file: `basePath / settingsFile`. */
    std::filesystem::path settingsPath() const { return basePath / settingsFile; }

    /** @return Absolute filesystem path to the preview image: `basePath / previewFile`. */
    std::filesystem::path previewPathFs() const { return basePath / previewFile; }

    /**
     * @brief Returns a WebView2 virtual-host URL for the preview image.
     *
     * The URL format is `https://animura.modules/<folder>/<file>`.
     * WebView2 resolves this via the `animura.modules` virtual host mapping
     * (set up in WebView2Host) to the local `modules/` directory.
     *
     * This is used by the React frontend for `<img src>` attributes —
     * it cannot use `file:///` URLs due to WebView2 security restrictions.
     *
     * @return Virtual-host URL string ready for use in an HTML `src` attribute.
     */
    QString previewPathQt() const {
        // Return a virtual-host URL that WebView2 resolves to the modules folder.
        return QStringLiteral("https://animura.modules/%1/%2")
            .arg(QString::fromStdWString(basePath.filename().wstring()),
                 QString::fromStdWString(previewFile.wstring()));
    }
};
