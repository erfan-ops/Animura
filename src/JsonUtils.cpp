/**
 * @file JsonUtils.cpp
 * @brief Thin utility for reading and parsing JSON files from disk.
 *
 * Wraps `std::ifstream` and `nlohmann::json::operator>>` in a single
 * convenience function used consistently by:
 * - ModuleCatalog — parsing `module.json` manifests.
 * - WallpaperController — reading `schema.json` and `settings.json`.
 * - SettingsSchemaValidator — loading schema and settings for validation.
 */

#include "JsonUtils.hpp"
#include <fstream>

bool JsonUtils::readJsonFile(const std::filesystem::path& path, nlohmann::json& out, std::string* error) {
    std::ifstream file(path);
    if (!file.is_open()) {
        if (error) *error = "Failed to open file: " + path.string();
        return false;
    }

    try {
        // nlohmann/json's stream operator reads and parses in one shot.
        file >> out;
        return true;
    }
    catch (const std::exception& e) {
        if (error) *error = e.what();
        return false;
    }
}
