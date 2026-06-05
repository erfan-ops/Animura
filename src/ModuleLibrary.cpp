#include "ModuleLibrary.hpp"
#include <sstream>
#include <iostream>
#include <filesystem>

ModuleLibrary::~ModuleLibrary() {
    unload();
}

bool ModuleLibrary::load(const std::wstring& dllPath, std::string& outError) {
    unload();

    std::wstring fullPath = std::filesystem::absolute(dllPath).wstring();
    m_lib = LoadLibraryExW(
        fullPath.c_str(),
        nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_APPLICATION_DIR
    );
    if (!m_lib) {
        DWORD err = GetLastError();

        LPWSTR buffer = nullptr;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            err,
            0,
            (LPWSTR)&buffer,
            0,
            nullptr
        );

        std::wstring message = buffer ? buffer : L"Unknown error";
        LocalFree(buffer);

        outError = "LoadLibraryEx failed. Error: " +
            std::to_string(err) +
            " - " +
            std::string(message.begin(), message.end());

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
