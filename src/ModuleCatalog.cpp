/**
 * @file ModuleCatalog.cpp
 * @brief Implementation of the module discovery system.
 *
 * ModuleCatalog scans the `./modules/` directory for subdirectories
 * containing valid `module.json` manifest files. Each valid module
 * directory produces one `ModuleInfo` struct. If zero modules are found,
 * the constructor throws — the application cannot function without at
 * least one module.
 */

#include "ModuleCatalog.hpp"
#include "JsonUtils.hpp"
#include <iostream>

using json = nlohmann::json;

/**
 * @brief Checks that a JSON object contains all required keys.
 *
 * Used to validate `module.json` manifests against the 6 required fields:
 * name, version, entry, schema, settings, preview.
 *
 * @param j       The JSON object to check.
 * @param keys    List of required key names.
 * @param missing Output — receives the name of the first missing key.
 * @return true if all required keys are present.
 */
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

        // Parse the module.json manifest.
        json metaJson;
        std::string err;
        if (!JsonUtils::readJsonFile(moduleJsonPath, metaJson, &err)) {
            std::cout << "Failed to read " << moduleJsonPath << ": " << err << "\n";
            continue;
        }

        // Validate all 6 required fields are present.
        std::string missing;
        if (!hasRequiredFields(metaJson, { "id","name","version","entry","schema","settings","preview" }, missing)) {
            std::cout << "Missing property \"" << missing << "\" in \"" << moduleJsonPath << "\"\n";
            continue;
        }

        ModuleInfo info{
            moduleDir.path(),
            metaJson["id"],
            metaJson["name"],
            metaJson.value("description", "N/A"),
            metaJson["version"],
            metaJson["entry"],
            metaJson["schema"],
            metaJson["settings"],
            metaJson["preview"]
        };

        // Verify all referenced files actually exist on disk.
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

bool ModuleCatalog::hasModuleId(const std::string& id) const {
    for (const auto& m : m_modules) {
        if (m.id == id) return true;
    }
    return false;
}

void ModuleCatalog::addModule(const ModuleInfo& info) {
    m_modules.push_back(info);
}
