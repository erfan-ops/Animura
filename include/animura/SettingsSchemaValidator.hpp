#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace SettingsSchemaValidator
{
    bool validate(
        const nlohmann::json& schema,
        const nlohmann::json& settings,
        std::string& outErrors);
}
