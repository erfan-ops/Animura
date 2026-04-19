#include "ModuleLibrary.hpp"
#include <sstream>

ModuleLibrary::~ModuleLibrary() {
    unload();
}

bool ModuleLibrary::load(const std::wstring& dllPath, std::string& outError) {
    unload();

    m_lib = LoadLibraryW(dllPath.c_str());
    if (!m_lib) {
        outError = "Could not load library: " + std::string(dllPath.begin(), dllPath.end());
        return false;
    }

    m_createFn = reinterpret_cast<CreateModuleFn>(GetProcAddress(m_lib, "createModule"));
    if (!m_createFn) {
        outError = "Function createModule not found";
        unload();
        return false;
    }

    return true;
}

void ModuleLibrary::unload() {
    if (m_lib) {
        FreeLibrary(m_lib);
        m_lib = nullptr;
        m_createFn = nullptr;
    }
}
