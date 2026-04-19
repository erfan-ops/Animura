// ModuleCatalog.cpp
#include "ModuleCatalog.hpp"
#include "JsonUtils.hpp"
#include <iostream>

using json = nlohmann::json;

static bool hasRequiredFields(const json& j, const std::vector<std::string>& keys, std::string& missing) {
    for (const auto& k : keys)
    {
        if (!j.contains(k)) { missing = k; return false; }
    }
    return true;
}

ModuleCatalog::ModuleCatalog(const std::filesystem::path& root) {
    scanModules(root);
    if (m_modules.empty())
        throw std::runtime_error("No modules were found");
}

void ModuleCatalog::scanModules(const std::filesystem::path& root) {
    if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root))
        throw std::runtime_error("Modules folder not found!");

    for (auto& moduleDir : std::filesystem::directory_iterator(root)) {
        if (moduleDir.is_regular_file()) continue;

        auto moduleJsonPath = moduleDir.path() / "module.json";
        if (!std::filesystem::exists(moduleJsonPath)) continue;

        json metaJson;
        std::string err;
        if (!JsonUtils::readJsonFile(moduleJsonPath, metaJson, &err)) {
            std::cout << "Failed to read " << moduleJsonPath << ": " << err << "\n";
            continue;
        }

        std::string missing;
        if (!hasRequiredFields(metaJson, { "name","version","entry","schema","settings","preview" }, missing)) {
            std::cout << "Missing property \"" << missing << "\" in \"" << moduleJsonPath << "\"\n";
            continue;
        }

        ModuleInfo info{
            moduleDir.path(),
            metaJson["name"],
            metaJson["version"],
            metaJson["entry"],
            metaJson["schema"],
            metaJson["settings"],
            metaJson["preview"]
        };

        if (!std::filesystem::exists(info.entryPath()) ||
            !std::filesystem::exists(info.schemaPath()) ||
            !std::filesystem::exists(info.settingsPath()) ||
            !std::filesystem::exists(info.previewPathFs()))
        {
            std::wcout << L"Skipping invalid module: " << moduleDir.path().wstring() << L"\n";
            continue;
        }

        m_modules.push_back(info);
    }
}
