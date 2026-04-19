#pragma once
#include "ModuleInfo.hpp"
#include <vector>

class ModuleCatalog {
public:
    explicit ModuleCatalog(const std::filesystem::path& root = "modules");

    const std::vector<ModuleInfo>& modules() const { return m_modules; }

private:
    std::vector<ModuleInfo> m_modules;
    void scanModules(const std::filesystem::path& root);
};
