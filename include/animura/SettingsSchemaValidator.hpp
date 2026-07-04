#pragma once
#include <nlohmann/json.hpp>
#include <string>

/**
 * @brief Validates module settings JSON against its JSON Schema before
 *        module loading.
 *
 * This validator catches type mismatches, range violations, and missing
 * keys early — before the module DLL is loaded — preventing crashes from
 * bad configuration.
 *
 * ## Validation Rules
 * | Rule          | Applies To              | Check                                     |
 * |---------------|-------------------------|-------------------------------------------|
 * | Missing key   | All                     | Every schema key must exist in settings   |
 * | Type mismatch | `int`, `float`, `bool`, `string` | Value must match declared type    |
 * | Below min     | `int`, `float`          | `value >= schema["min"]`                  |
 * | Above max     | `int`, `float`          | `value <= schema["max"]`                  |
 * | Invalid option| `select`                | Value must be in `schema["options"]`      |
 *
 * ## Limitations (by design)
 * - Unknown keys in settings are silently ignored — modules may have
 *   undocumented settings.
 * - Unknown types pass validation — only `int`, `float`, `bool`, and
 *   `string` are type-checked.
 * - No cross-field validation (each field is checked independently).
 * - No string constraints (no minLength, maxLength, or pattern).
 * - Color and color_list types are not checked here — they are validated
 *   by the React UI controls.
 *
 * @see WallpaperController::validateModuleSettings() for the caller.
 */
namespace SettingsSchemaValidator
{
    /**
     * @brief Recursively validates module settings against a JSON schema.
     *
     * Walks the schema tree depth-first. For each key in the schema:
     * - If the key is missing from settings, reports an error.
     * - If the schema node has a `"type"` field, checks type, min/max,
     *   and options constraints.
     * - If the schema node is a plain object (no `"type"`), recurses into
     *   the corresponding settings subtree.
     *
     * All errors are accumulated into a multi-line string so the caller
     * can report every issue at once rather than failing on the first.
     *
     * @param schema    The JSON Schema object (from `schema.json`).
     * @param settings  The settings values to validate (from `settings.json`).
     * @param outErrors Receives a multi-line error string listing every
     *                  validation failure. Empty if validation passes.
     *
     * @return true if all settings conform to the schema.
     */
    bool validate(
        const nlohmann::json& schema,
        const nlohmann::json& settings,
        std::string& outErrors);
}
