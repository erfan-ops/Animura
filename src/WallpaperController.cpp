/**
 * @file WallpaperController.cpp
 * @brief Implementation of the central wallpaper lifecycle orchestrator.
 *
 * WallpaperController manages the full lifecycle of wallpaper modules:
 * discovery, DLL loading, validation, worker thread spawning, render loop
 * execution, and shutdown. It is the bridge between the React UI (via
 * NativeBridge COM calls) and the wallpaper module DLLs.
 *
 * ## Threading Architecture
 * - This class lives on the **Qt main thread**.
 * - Public methods are called from NativeBridge (marshaled from WebView2
 *   threads via QMetaObject::invokeMethod with BlockingQueuedConnection).
 * - Module execution runs on a dedicated **worker thread** (m_worker)
 *   because many rendering libraries require thread affinity for
 *   initialization, context management, rendering, and teardown.
 * - `stop()` sets an atomic flag from the main thread; the worker observes
 *   it and exits the render loop.
 * - Module destruction happens inside the worker lambda after `run()`
 *   returns, ensuring rendering library teardown on the correct thread.
 */

#include "WallpaperController.hpp"
#include "AppSettings.hpp"
#include "JsonUtils.hpp"
#include "SettingsSchemaValidator.hpp"
#include "ZipExtractor.hpp"
#include "animura/desktop_host_resolver.hpp"
#include "animura/wallpaper_host.hpp"

#include <filesystem>

#include <QJsonDocument>
#include <QFile>
#include <QSaveFile>
#include <QDebug>


using json = nlohmann::json;

// ── Constructor / Destructor ──

WallpaperController::WallpaperController(QObject* parent)
    : QObject(parent), m_catalog("modules")
{
    m_wallpaperBridge = std::make_unique<animura::desktop::SystemWallpaperBridge>();
}

WallpaperController::~WallpaperController() {
    stopWallpaper();
    m_wallpaperBridge.reset();
}

// ── Public API (Q_INVOKABLE) ──

int WallpaperController::getRunningModuleId() const {
    return m_running ? m_currentModuleId : -1;
}

QVariantList WallpaperController::getModulesList() const {
    QVariantList list;
    const auto& mods = m_catalog.modules();
    for (int i = 0; i < static_cast<int>(mods.size()); ++i) {
        const auto& m = mods[i];
        QVariantMap item;
        item["name"] = QString::fromStdString(m.name);
        item["description"] = QString::fromStdString(m.description);
        item["version"] = QString::fromStdString(m.version);
        item["previewPath"] = m.previewPathQt();
        item["id"] = i;
        list.append(item);
    }
    return list;
}

// ── Validation Helpers ──

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

// ── DLL Loading ──

bool WallpaperController::ensureLibraryLoaded(const ModuleInfo& info, int moduleIndex) {
    // If the same module is already loaded, skip the reload.
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

// ── Worker Thread ──

void WallpaperController::startWorker(const ModuleInfo& info) {
    // Save the original wallpaper path before the module takes over the desktop.
    m_originalWallpaper = m_wallpaperBridge->GetWallpaper();
    if (m_originalWallpaper.empty()) {
        qWarning() << "Warning: Could not retrieve current wallpaper path.";
    }

    m_running = true;
    // A wallpaper always starts attached to the desktop; the worker thread
    // will correct this if AttachWindowToDesktop fails.  Setting it early
    // on the main thread ensures that the frontend sees the correct value
    // when it polls getIsAttached() after startWallpaper() returns.
    m_attached = true;
    emit runningModuleChanged();

    /**
     * Worker thread lambda — OWNs the module lifecycle.
     *
     * This is the ONLY thread where rendering library operations are
     * allowed. The module is created here, run here, and crucially
     * DESTROYED here (after run() returns), ensuring that any teardown
     * with thread affinity requirements (e.g., GLFW's
     * `glfwDestroyWindow()`/`glfwTerminate()`) executes on the same
     * thread that performed initialization.
     */
    m_worker = std::thread([this, info]() {
        try {
            // Read the settings file as raw bytes and pass them directly
            // to the module — no JSON parse/dump round-trip that could
            // corrupt multi-byte UTF-8 sequences.
            QFile f(QString::fromStdWString(info.settingsPath().wstring()));
            if (!f.open(QIODevice::ReadOnly)) {
                throw std::runtime_error(
                    "Failed to open settings: " +
                    info.settingsPath().string());
            }
            const QByteArray raw = f.readAll();
            f.close();

            // Call the DLL's factory function to create the module.
            // Ownership transfers to m_module (unique_ptr).
            m_module.reset(m_library.createFn()(raw.constData()));
            if (!m_module) {
                throw std::runtime_error("Module factory returned null.");
            }

            // Resolve the desktop host using the new robust architecture.
            animura::desktop::DesktopHostResolver resolver;
            auto hostInfo = resolver.Resolve();
            if (!hostInfo.IsValid()) {
                qCritical() << "Failed to resolve desktop wallpaper host.";
                m_attached = false;
                emit attachedChanged();
                m_module.reset();
                return;
            }

            // Store the host attachment object (accessed from main & worker threads).
            m_wallpaperHost = std::make_unique<animura::desktop::WallpaperHost>(
                std::move(hostInfo));

            // Prepare Window — show it and attach to the desktop host.
            SetWindowLongPtrW(m_module->hwnd(), GWL_STYLE, WS_VISIBLE);
            bool ok = m_wallpaperHost->Attach(m_module->hwnd());
            m_attached = ok;
            emit attachedChanged();

            if (!ok) {
                qCritical() << "Failed to attach wallpaper window to desktop host.";
                m_wallpaperHost.reset();
                m_module.reset();
                return;
            }

            // Blocking call: module runs until stop() is called from the main thread.
            m_module->run();

            m_module.reset();
        }
        catch (const std::exception& e) {
            QString errorMsg = QString::fromStdString(e.what());
            qCritical() << "CRITICAL: Module Thread Crash:" << errorMsg;
        }
        });
}

// ── Start / Stop ──

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

    // Persist this module as the most recently used wallpaper.
    // The app uses this to auto-restore on next launch if the user
    // has enabled that preference.
    AppSettings::instance().setLastUsedWallpaperID(info.id);
}

void WallpaperController::stopWallpaper() {
    if (!m_running) return;

    // 1. Detach and Stop the module instance
    if (m_module) {
        // Try to hide it immediately if it's still alive
        if (IsWindow(m_module->hwnd())) {
            SetWindowLongPtrW(m_module->hwnd(), GWL_STYLE, ~WS_VISIBLE);
            if (m_wallpaperHost) {
                m_wallpaperHost->Detach();
            }
        }
        m_module->stop();
    }

    // 2. Safely Join the thread
    // Note: If stopWallpaper is called FROM the worker thread (via invokeMethod),
    // we MUST NOT join the thread to itself.
    if (m_worker.joinable() && std::this_thread::get_id() != m_worker.get_id()) {
        m_worker.join();
    }
    else if (m_worker.joinable()) {
        m_worker.detach(); // Safety fallback
    }

    // 3. Reset state
    m_running = false;
    m_attached = false;
    m_wallpaperHost.reset();
    emit runningModuleChanged();
    emit attachedChanged();

    // 4. Restore the Windows Desktop wallpaper
    restoreWallpaper();

    qInfo() << "Wallpaper stopped and original restored.";
}

void WallpaperController::restoreWallpaper() {
    m_wallpaperBridge->SetWallpaper(m_originalWallpaper);
}

// ── Settings I/O ──

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

// ── Attach / Detach ──

void WallpaperController::detachWallpaper() {
    if (!m_running || !m_attached) return;
    if (!m_module) return;

    HWND hwnd = m_module->hwnd();
    if (!IsWindow(hwnd)) return;

    if (m_wallpaperHost) {
        m_wallpaperHost->Detach();
    }

    // Position the detached window at the top-left of the virtual screen.
    SetWindowPos(
        hwnd,
        HWND_BOTTOM,
        GetSystemMetrics(SM_XVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN),
        0, 0,
        SWP_NOSIZE | SWP_NOZORDER
    );

    restoreWallpaper();

    m_attached = false;
    emit attachedChanged();

    qInfo() << "Wallpaper detached from desktop.";
}

void WallpaperController::attachWallpaper() {
    if (!m_running || m_attached) return;
    if (!m_module) return;

    HWND hwnd = m_module->hwnd();
    if (!IsWindow(hwnd)) return;

    // Re-resolve the desktop host in case Explorer restarted since detach.
    animura::desktop::DesktopHostResolver resolver;
    auto hostInfo = resolver.Resolve();
    if (!hostInfo.IsValid()) {
        qWarning() << "Failed to re-resolve desktop host for re-attachment.";
        return;
    }

    m_wallpaperHost = std::make_unique<animura::desktop::WallpaperHost>(
        std::move(hostInfo));

    if (!m_wallpaperHost->Attach(hwnd)) {
        qWarning() << "Failed to re-attach wallpaper window to desktop.";
        m_wallpaperHost.reset();
        return;
    }

    m_attached = true;
    emit attachedChanged();

    qInfo() << "Wallpaper attached to desktop.";
}

bool WallpaperController::getIsAttached() const {
    return m_running && m_attached;
}

// ── Runtime Module Installation ──

QString WallpaperController::installModuleFromPath(const QString& zipPath) {
    if (zipPath.isEmpty()) {
        return {};
    }

    std::filesystem::path zipFsPath(
        zipPath.toStdWString());
    std::string err;

    // ── Step 1 & 2: Verify module.json exists at the ZIP root ──
    if (!ZipExtractor::HasEntry(zipFsPath, "module.json")) {
        return QStringLiteral(
            "Invalid module package: module.json not found at the ZIP root.");
    }

    // Verify module.json is at the root (not in a subdirectory).
    std::vector<std::string> rootEntries;
    if (!ZipExtractor::ListRootEntries(zipFsPath, rootEntries, err)) {
        return QString::fromStdString(
            "Failed to read ZIP: " + err);
    }
    bool hasModuleJson = false;
    for (const auto& entry : rootEntries) {
        if (entry == "module.json") {
            hasModuleJson = true;
            break;
        }
    }
    if (!hasModuleJson) {
        return QStringLiteral(
            "Invalid module package: module.json must be at the ZIP root, not in a subdirectory.");
    }

    // ── Step 3: Read module.json directly from the ZIP into memory ──
    auto moduleJsonContent = ZipExtractor::ReadFile(zipFsPath, "module.json");
    if (!moduleJsonContent.has_value()) {
        return QStringLiteral(
            "Failed to read module.json from ZIP.");
    }

    nlohmann::json metaJson;
    try {
        metaJson = nlohmann::json::parse(*moduleJsonContent);
    } catch (const std::exception& e) {
        return QString::fromStdString(
            std::string("Failed to parse module.json: ") + e.what());
    }

    // Validate 'id' field.
    if (!metaJson.contains("id") || !metaJson["id"].is_string()) {
        return QStringLiteral(
            "Invalid module.json: missing or invalid \"id\" field.");
    }

    std::string moduleId = metaJson["id"].get<std::string>();
    if (moduleId.empty()) {
        return QStringLiteral(
            "Invalid module.json: \"id\" must not be empty.");
    }

    // Validate 'name' field (needed for UI display).
    if (!metaJson.contains("name") || !metaJson["name"].is_string()) {
        return QStringLiteral(
            "Invalid module.json: missing or invalid \"name\" field.");
    }

    // ── Step 4: Check for duplicate module ID ──
    if (m_catalog.hasModuleId(moduleId)) {
        return QString::fromStdString(
            "A module with id \"" + moduleId + "\" is already installed.");
    }

    // ── Step 5: Extract the full ZIP to modules/<id>/ ──
    // Resolve destination relative to the executable's location.
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::filesystem::path modulesDir(exePath);
    modulesDir = modulesDir.parent_path() / "modules" / moduleId;

    if (!ZipExtractor::ExtractAll(zipFsPath, modulesDir, err)) {
        // Clean up partial extraction.
        std::error_code ec;
        std::filesystem::remove_all(modulesDir, ec);
        return QString::fromStdString(
            "Failed to extract module: " + err);
    }

    // ── Step 6: Build ModuleInfo and update catalog ──
    // Read the extracted module.json again (now at its final location)
    // to get all required fields for ModuleInfo.
    nlohmann::json finalMetaJson;
    std::filesystem::path finalJsonPath = modulesDir / "module.json";
    if (!JsonUtils::readJsonFile(finalJsonPath, finalMetaJson, &err)) {
        // Clean up — module.json must be readable after extraction.
        std::error_code ec;
        std::filesystem::remove_all(modulesDir, ec);
        return QString::fromStdString(
            "Failed to read extracted module.json: " + err);
    }

    // Validate all required ModuleInfo fields.
    static const std::vector<std::string> kRequiredFields = {
        "id", "name", "version", "entry", "schema", "settings", "preview"
    };
    for (const auto& field : kRequiredFields) {
        if (!finalMetaJson.contains(field)) {
            std::error_code ec;
            std::filesystem::remove_all(modulesDir, ec);
            return QString::fromStdString(
                "Invalid module.json: missing required field \"" + field + "\".");
        }
    }

    ModuleInfo info{
        modulesDir,
        finalMetaJson["id"].get<std::string>(),
        finalMetaJson["name"].get<std::string>(),
        finalMetaJson["version"].get<std::string>(),
        finalMetaJson["entry"].get<std::string>(),
        finalMetaJson["schema"].get<std::string>(),
        finalMetaJson["settings"].get<std::string>(),
        finalMetaJson["preview"].get<std::string>()
    };

    // Verify the referenced files actually exist.
    if (!std::filesystem::exists(info.entryPath())
        || !std::filesystem::exists(info.schemaPath())
        || !std::filesystem::exists(info.settingsPath())
        || !std::filesystem::exists(info.previewPathFs())) {
        std::error_code ec;
        std::filesystem::remove_all(modulesDir, ec);
        return QStringLiteral(
            "Installed module is missing one or more required files.");
    }

    m_catalog.addModule(info);

    // ── Step 7: Notify the frontend ──
    emit modulesChanged();

    qInfo() << "Module installed:" << QString::fromStdString(moduleId);
    return {}; // Success — empty string.
}

// ── App Settings ──

bool WallpaperController::getRestoreLastWallpaper() const {
    return AppSettings::instance().restoreLastWallpaper();
}

void WallpaperController::setRestoreLastWallpaper(bool enable) {
    AppSettings::instance().setRestoreLastWallpaper(enable);
}

int WallpaperController::findModuleIndexById(const std::string& id) const {
    const auto& mods = m_catalog.modules();
    for (int i = 0; i < static_cast<int>(mods.size()); ++i) {
        if (mods[i].id == id) {
            return i;
        }
    }
    return -1;
}
