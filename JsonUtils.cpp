#include "JsonUtils.hpp"
#include <fstream>

bool JsonUtils::readJsonFile(const std::filesystem::path& path, nlohmann::json& out, std::string* error) {
    std::ifstream file(path);
    if (!file.is_open()) {
        if (error) *error = "Failed to open file: " + path.string();
        return false;
    }

    try {
        file >> out;
        return true;
    }
    catch (const std::exception& e) {
        if (error) *error = e.what();
        return false;
    }
}
