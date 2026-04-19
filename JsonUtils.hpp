#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>

namespace JsonUtils
{
    bool readJsonFile(const std::filesystem::path& path, nlohmann::json& out, std::string* error = nullptr);
}
