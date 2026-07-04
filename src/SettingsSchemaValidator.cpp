/**
 * @file SettingsSchemaValidator.cpp
 * @brief Recursive JSON schema validator for module settings.
 *
 * Validates a module's `settings.json` against its `schema.json` before
 * the module DLL is loaded. Catches type mismatches, range violations,
 * missing keys, and invalid options early — preventing crashes from bad
 * configuration.
 *
 * The validator walks the schema tree depth-first. For each key:
 * - If the key is missing from settings, an error is reported.
 * - If the schema node has a `"type"` field, type, min/max, and options
 *   constraints are checked.
 * - If the node is a plain object (no `"type"`), the validator recurses
 *   into the corresponding settings subtree.
 *
 * All errors are accumulated into one multi-line string so the caller
 * can report every issue at once.
 */

#include "SettingsSchemaValidator.hpp"
#include <sstream>
#include <vector>

using json = nlohmann::json;

/**
 * @brief Joins a path vector into a dotted string for error messages.
 *
 * Example: `["stars", "color"]` → `"stars.color"`
 */
static std::string joinPath(const std::vector<std::string>& path) {
    std::string out;
    for (size_t i = 0; i < path.size(); ++i) {
        out += path[i];
        if (i + 1 < path.size()) out += ".";
    }
    return out;
}

/**
 * @brief Checks whether a JSON value matches the declared schema type.
 *
 * - `"int"`    — must be `is_number_integer()`.
 * - `"float"`  — must be `is_number()` (accepts integers too).
 * - `"bool"`   — must be `is_boolean()`.
 * - `"string"` — must be `is_string()`.
 * - Unknown types pass through (validated by the React UI, not here).
 */
static bool checkType(const json& value, const std::string& type) {
    if (type == "int") return value.is_number_integer();
    if (type == "float") return value.is_number();
    if (type == "bool") return value.is_boolean();
    if (type == "string") return value.is_string();
    return true;
}

/**
 * @brief Recursively validates a settings node against a schema node.
 *
 * This is the core validation function. It iterates every key in the
 * schema, checks its presence in settings, validates type and constraints,
 * and recurses into nested objects.
 *
 * @param schemaNode   The schema subtree to validate against.
 * @param settingsNode The settings subtree to validate.
 * @param path         Current path in the schema tree (for error messages).
 * @param errors       Accumulated error stream.
 * @return true if the subtree is valid.
 */
static bool validateNode(
    const json& schemaNode,
    const json& settingsNode,
    std::vector<std::string>& path,
    std::stringstream& errors)
{
    bool ok = true;

    for (auto it = schemaNode.begin(); it != schemaNode.end(); ++it) {
        const std::string key = it.key();
        const json& schemaValue = it.value();

        path.push_back(key);

        if (!settingsNode.contains(key)) {
            errors << "Missing setting: " << joinPath(path) << "\n";
            path.pop_back();
            ok = false;
            continue; // do not access missing node
        }

        const json& settingValue = settingsNode[key];

        if (schemaValue.is_object() && schemaValue.contains("type")) {
            std::string type = schemaValue["type"];

            // Type check.
            if (!checkType(settingValue, type)) {
                errors << "Type mismatch at: " << joinPath(path) << "\n";
                ok = false;
            }

            // Min constraint (for int and float).
            if (schemaValue.contains("min") && settingValue.is_number() &&
                settingValue < schemaValue["min"])
            {
                errors << "Value below min at: " << joinPath(path) << "\n";
                ok = false;
            }

            // Max constraint (for int and float).
            if (schemaValue.contains("max") && settingValue.is_number() &&
                settingValue > schemaValue["max"])
            {
                errors << "Value above max at: " << joinPath(path) << "\n";
                ok = false;
            }

            // Options constraint (for select type).
            if (schemaValue.contains("options")) {
                bool valid = false;
                for (const auto& opt : schemaValue["options"]) {
                    if (opt == settingValue) { valid = true; break; }
                }
                if (!valid) {
                    errors << "Invalid option at: " << joinPath(path) << "\n";
                    ok = false;
                }
            }
        }
        else if (schemaValue.is_object()) {
            // Nested object (no "type" field) — recurse into it.
            ok &= validateNode(schemaValue, settingValue, path, errors);
        }

        path.pop_back();
    }

    return ok;
}

bool SettingsSchemaValidator::validate(
    const json& schema,
    const json& settings,
    std::string& outErrors)
{
    std::vector<std::string> path;
    std::stringstream ss;
    bool ok = validateNode(schema, settings, path, ss);
    outErrors = ss.str();
    return ok;
}
