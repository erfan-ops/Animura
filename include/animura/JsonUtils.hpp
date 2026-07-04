#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>

/**
 * @brief Thin utility for reading JSON files with error reporting.
 *
 * Wraps `std::ifstream` + `nlohmann::json::operator>>` in a single
 * convenience function used consistently by ModuleCatalog,
 * WallpaperController, and SettingsSchemaValidator.
 */
namespace JsonUtils
{
    /**
     * @brief Reads and parses a JSON file from disk.
     *
     * Opens the file via `std::ifstream`, then streams it into an
     * `nlohmann::json` object using the library's stream operator.
     * Parsing exceptions are caught and reported through the optional
     * error output parameter.
     *
     * @param path  Filesystem path to the JSON file.
     * @param out   Receives the parsed JSON object.
     * @param error Optional output parameter. On failure, receives a
     *              description of the error (open failure or parse
     *              exception message). Pass `nullptr` to ignore errors.
     *
     * @return true if the file was opened and parsed successfully.
     */
    bool readJsonFile(const std::filesystem::path& path, nlohmann::json& out, std::string* error = nullptr);
}
