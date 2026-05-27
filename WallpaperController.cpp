#include "WallpaperController.hpp"
#include "JsonUtils.hpp"
#include "SettingsSchemaValidator.hpp"

#include <filesystem>

#include <QJsonDocument>
#include <QFile>
#include <QSaveFile>
#include <QDebug>


#define WALLPAPER_HOST_STATIC
#include <wallpaper-host/desktop_utils.hpp>
using json = nlohmann::json;

WallpaperController::WallpaperController(QObject* parent)
    : QObject(parent), m_catalog("modules")
{
    wallpaper::desktop::initSetWallpaperMethod();
}

WallpaperController::~WallpaperController() {
    stopWallpaper();
    wallpaper::desktop::shutdownWallpaperMethod();
}

QVariantList WallpaperController::getModulesList() const {
    QVariantList list;
    const auto& mods = m_catalog.modules();
    for (int i = 0; i < static_cast<int>(mods.size()); ++i) {
        const auto& m = mods[i];
        QVariantMap item;
        item["name"] = QString::fromStdString(m.name);
        item["version"] = QString::fromStdString(m.version);
        item["previewPath"] = m.previewPathQt();
        item["id"] = i;
        list.append(item);
    }
    return list;
}

bool WallpaperController::validateIndex(int moduleIndex) const {
    return moduleIndex >= 0 && moduleIndex < static_cast<int>(m_catalog.modules().size());
}

bool WallpaperController::validateModuleFiles(const ModuleInfo& info) const {
    if (!std::filesystem::exists(info.entryPath())) {
        qWarning() << "Module dll not found:" << QString::fromStdWString(info.entryPath().wstring());
        return false;
    }
    if (!std::filesystem::exists(info.schemaPath())) {
        qWarning() << "Schema file not found:" << QString::fromStdWString(info.schemaPath().wstring());
        return false;
    }
    if (!std::filesystem::exists(info.settingsPath())) {
        qWarning() << "Settings file not found:" << QString::fromStdWString(info.settingsPath().wstring());
        return false;
    }
    return true;
}

bool WallpaperController::validateModuleSettings(const ModuleInfo& info) const {
    json schemaJson, settingsJson;
    std::string err;

    if (!JsonUtils::readJsonFile(info.schemaPath(), schemaJson, &err)) {
        qWarning() << "Failed to read schema:" << err.c_str();
        return false;
    }
    if (!JsonUtils::readJsonFile(info.settingsPath(), settingsJson, &err)) {
        qWarning() << "Failed to read settings:" << err.c_str();
        return false;
    }

    std::string validationErrors;
    if (!SettingsSchemaValidator::validate(schemaJson, settingsJson, validationErrors)) {
        qWarning() << validationErrors.c_str();
        return false;
    }
    return true;
}

bool WallpaperController::ensureLibraryLoaded(const ModuleInfo& info, int moduleIndex) {
    if (moduleIndex == m_currentModuleId && m_library.isLoaded())
        return true;

    if (m_running) stopWallpaper();
    m_library.unload();

    std::string err;
    if (!m_library.load(info.entryPath().wstring(), err)) {
        qWarning() << err.c_str();
        return false;
    }

    m_currentModuleId = moduleIndex;
    return true;
}

void WallpaperController::startWorker(const ModuleInfo& info) {
    m_originalWallpaper = wallpaper::desktop::GetCurrentWallpaperPath();
    if (m_originalWallpaper.empty()) {
        qWarning() << "No wallpaper found.";
    }

    m_running = true;

    m_worker = std::thread([this, info]()
        {
            try {
                json settingsJson;
                std::string err;
                if (!JsonUtils::readJsonFile(info.settingsPath(), settingsJson, &err)) {
                    qWarning() << err.c_str();
                    return;
                }

                std::string dump = settingsJson.dump();
                m_module.reset(m_library.createFn()(dump.c_str()));
                if (!m_module) {
                    qWarning() << "createModule returned null";
                    return;
                }

                SetWindowLongPtrW(m_module->hwnd(), GWL_STYLE, WS_VISIBLE);
                wallpaper::desktop::AttachWindowToDesktop(m_module->hwnd());

                m_module->run();
            }
            catch (const std::exception& e) {
                qWarning() << "Exception in module thread:" << e.what();
            }
        });
}

void WallpaperController::startWallpaper(int moduleIndex) {
    if (!validateIndex(moduleIndex)) {
        qWarning() << "Invalid module index:" << moduleIndex;
        return;
    }

    const ModuleInfo& info = m_catalog.modules()[moduleIndex];

    if (!validateModuleFiles(info)) return;
    if (!validateModuleSettings(info)) return;
    if (!ensureLibraryLoaded(info, moduleIndex)) return;

    if (m_running) stopWallpaper();
    startWorker(info);
}

void WallpaperController::stopWallpaper() {
    if (!m_running) return;

    if (m_module) {
        SetWindowLongPtrW(m_module->hwnd(), GWL_STYLE, ~WS_VISIBLE);
        wallpaper::desktop::DetachWindowFromDesktop(m_module->hwnd());
        m_module->stop();
    }

    if (m_worker.joinable())
        m_worker.join();

    m_running = false;

    m_module.reset();

    restoreWallpaper();
}

void WallpaperController::restoreWallpaper() const {
    wallpaper::desktop::SetWallpaper(m_originalWallpaper);
}

QJsonObject WallpaperController::loadSettingsUI(int moduleIndex) {
    if (!validateIndex(moduleIndex)) return {};

    const ModuleInfo& info = m_catalog.modules()[moduleIndex];

    QFile schemaFile(QString::fromStdWString(info.schemaPath().wstring()));
    QFile settingsFile(QString::fromStdWString(info.settingsPath().wstring()));

    QJsonObject schema, settings;
    if (schemaFile.open(QIODevice::ReadOnly))
        schema = QJsonDocument::fromJson(schemaFile.readAll()).object();
    if (settingsFile.open(QIODevice::ReadOnly))
        settings = QJsonDocument::fromJson(settingsFile.readAll()).object();

    QJsonObject out;
    out["schema"] = schema;
    out["settings"] = settings;
    return out;
}

void WallpaperController::applySettings(int moduleIndex, QJsonObject settings) {
    if (!validateIndex(moduleIndex)) return;
    const ModuleInfo& info = m_catalog.modules()[moduleIndex];

    QString filePath = QString::fromStdWString(info.settingsPath().wstring());
    QSaveFile file(filePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        qWarning() << "Failed to open settings file:" << filePath
            << "Error:" << file.errorString();
        return;
    }

    QJsonDocument doc(settings);
    file.write(doc.toJson(QJsonDocument::Compact));

    if (!file.commit())
        qWarning() << "Failed to save settings:" << file.errorString();
}
