#include "SettingsSchemaValidator.hpp"
#include <sstream>
#include <vector>

using json = nlohmann::json;

static std::string joinPath(const std::vector<std::string>& path) {
    std::string out;
    for (size_t i = 0; i < path.size(); ++i) {
        out += path[i];
        if (i + 1 < path.size()) out += ".";
    }
    return out;
}

static bool checkType(const json& value, const std::string& type) {
    if (type == "int") return value.is_number_integer();
    if (type == "float") return value.is_number();
    if (type == "bool") return value.is_boolean();
    if (type == "string") return value.is_string();
    return true;
}

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

            if (!checkType(settingValue, type)) {
                errors << "Type mismatch at: " << joinPath(path) << "\n";
                ok = false;
            }

            if (schemaValue.contains("min") && settingValue.is_number() &&
                settingValue < schemaValue["min"])
            {
                errors << "Value below min at: " << joinPath(path) << "\n";
                ok = false;
            }

            if (schemaValue.contains("max") && settingValue.is_number() &&
                settingValue > schemaValue["max"])
            {
                errors << "Value above max at: " << joinPath(path) << "\n";
                ok = false;
            }

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
