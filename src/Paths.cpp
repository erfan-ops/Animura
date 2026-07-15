/**
 * @file Paths.cpp
 * @brief Implementation of the standardized application filesystem paths.
 *
 * Uses the Windows `SHGetKnownFolderPath` API to resolve the Local AppData
 * directory and creates the `Animura/` subdirectory underneath it.
 * Application-scoped persistent files live under this directory.
 */

#include "Paths.hpp"

#include <Windows.h>
#include <shlobj.h>
#include <stdexcept>


bool Paths::initialized{false};
std::filesystem::path Paths::appData_{};
std::filesystem::path Paths::settings_{};

namespace {

/** Filename for the application-scoped settings file. */
constexpr const char* kSettingsFileName = "settings.json";

/** Subdirectory name under Local AppData. */
constexpr const wchar_t* kAppDirName = L"Animura";

/**
 * @brief Resolves the Local AppData path and creates the Animura
 *        subdirectory.
 *
 * Calls `SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, ...)`
 * to obtain the user's local application data directory, appends the
 * Animura subdirectory, and ensures it exists on disk.
 *
 * @return Absolute path to `%LOCALAPPDATA%/Animura/`.
 *
 * @throws std::runtime_error if the shell API call fails.
 */
std::filesystem::path resolveAppDataPath()
{
    PWSTR path = nullptr;

    HRESULT hr = SHGetKnownFolderPath(
        FOLDERID_LocalAppData,
        KF_FLAG_CREATE,
        nullptr,
        &path);

    if (FAILED(hr))
        throw std::runtime_error("Failed to get LocalAppData path.");

    std::filesystem::path result(path);
    CoTaskMemFree(path);

    result /= kAppDirName;
    std::filesystem::create_directories(result);

    return result;
}

} // namespace

void Paths::init() {
    Paths::appData_ = resolveAppDataPath();
    Paths::settings_ = Paths::appData_ / kSettingsFileName;
    Paths::initialized = true;
}

std::filesystem::path Paths::appData() {
    if (!Paths::initialized) {
        Paths::init();
    }
    return Paths::appData_;
}

std::filesystem::path Paths::settings() {
    if (!Paths::initialized) {
        Paths::init();
    }
    return Paths::settings_;
}
