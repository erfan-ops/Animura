/**
 * @file AppSettings.cpp
 * @brief Implementation of the application-scoped persistent settings
 *        manager.
 *
 * AppSettings owns the `settings.json` file under
 * `%LOCALAPPDATA%/Animura/`. It provides a clean API for reading and
 * writing application preferences while hiding all JSON and file I/O
 * details.
 *
 * ## File I/O
 * - **Read**: Uses `std::ifstream` with RAII — the stream destructor
 *   closes the file automatically.
 * - **Write**: Uses `QSaveFile` for atomic writes — data is written to a
 *   temporary file and renamed over the target on commit, preventing
 *   corruption from mid-write crashes.
 *
 * ## Validation
 * On load, the JSON is checked for required keys and correct types.
 * If validation fails, the entire file is recreated from defaults rather
 * than partially repaired.
 */

#include "AppSettings.hpp"
#include "Paths.hpp"

#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

#include <fstream>
#include <filesystem>

// ── Singleton ──

AppSettings& AppSettings::instance() {
    static AppSettings s;
    return s;
}

// ── Initialization ──

void AppSettings::initialize() {
    if (m_initialized) return;

    if (!loadFromDisk()) {
        qWarning() << "AppSettings: failed to load settings, recreating defaults.";
        createDefault();
    }

    m_initialized = true;
}

// ── Getters ──

bool AppSettings::restoreLastWallpaper() const {
    return m_data.value("restoreLastWallpaper", false);
}

std::string AppSettings::lastUsedWallpaperID() const {
    return m_data.value("lastUsedWallpaperID", "delaunay-flow");
}

// ── Setters ──

void AppSettings::setRestoreLastWallpaper(bool value) {
    m_data["restoreLastWallpaper"] = value;
    save();
}

void AppSettings::setLastUsedWallpaperID(const std::string& id) {
    m_data["lastUsedWallpaperID"] = id;
    save();
}

// ── Save (atomic via QSaveFile) ──

void AppSettings::save() {
    const std::filesystem::path filePath = Paths::settings();

    // Ensure the parent directory exists.
    std::error_code ec;
    std::filesystem::create_directories(filePath.parent_path(), ec);

    QSaveFile file(QString::fromStdWString(filePath.wstring()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "AppSettings: failed to open settings file for writing:"
                    << file.errorString();
        return;
    }

    const std::string jsonStr = m_data.dump(4);
    file.write(jsonStr.c_str(), static_cast<qint64>(jsonStr.size()));

    if (!file.commit()) {
        qWarning() << "AppSettings: failed to commit settings file:"
                    << file.errorString();
    }
}

// ── Private Helpers ──

bool AppSettings::loadFromDisk() {
    const std::filesystem::path filePath = Paths::settings();

    if (!std::filesystem::exists(filePath)) {
        qInfo() << "AppSettings: settings file not found, will create defaults.";
        createDefault();
        return true; // Defaults created successfully.
    }

    std::ifstream file(filePath);
    if (!file.is_open()) {
        qWarning() << "AppSettings: failed to open settings file for reading.";
        return false;
    }

    nlohmann::json parsed;
    try {
        file >> parsed;
    } catch (const std::exception& e) {
        qWarning() << "AppSettings: JSON parse error:" << e.what();
        return false;
    }

    // file closes automatically via RAII when the ifstream destructor runs.

    if (!validate(parsed)) {
        qWarning() << "AppSettings: settings validation failed.";
        return false;
    }

    m_data = std::move(parsed);
    return true;
}

bool AppSettings::validate(const nlohmann::json& j) {
    // restoreLastWallpaper — must exist and be boolean.
    if (!j.contains("restoreLastWallpaper") || !j["restoreLastWallpaper"].is_boolean()) {
        qWarning() << "AppSettings::validate: missing or invalid 'restoreLastWallpaper' (expected boolean).";
        return false;
    }

    // lastUsedWallpaperID — must exist and be string.
    if (!j.contains("lastUsedWallpaperID") || !j["lastUsedWallpaperID"].is_string()) {
        qWarning() << "AppSettings::validate: missing or invalid 'lastUsedWallpaperID' (expected string).";
        return false;
    }

    return true;
}

void AppSettings::createDefault() {
    m_data = nlohmann::json::object();
    m_data["restoreLastWallpaper"] = false;
    m_data["lastUsedWallpaperID"] = "delaunay-flow";
    save();
}
