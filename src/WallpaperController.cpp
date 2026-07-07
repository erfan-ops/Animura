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
 *   because GLFW/OpenGL require thread affinity for init, context,
 *   rendering, and teardown.
 * - `stop()` sets an atomic flag from the main thread; the worker observes
 *   it and exits the render loop.
 * - Module destruction happens inside the worker lambda after `run()`
 *   returns, ensuring GLFW teardown on the correct thread.
 */

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

// ── Constructor / Destructor ──

WallpaperController::WallpaperController(QObject* parent)
    : QObject(parent), m_catalog("modules")
{
    wallpaper::desktop::initSetWallpaperMethod();
}

WallpaperController::~WallpaperController() {
    stopWallpaper();
    wallpaper::desktop::shutdownWallpaperMethod();
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
    m_originalWallpaper = wallpaper::desktop::GetCurrentWallpaperPath();
    if (m_originalWallpaper.empty()) {
        qWarning() << "Warning: Could not retrieve current wallpaper path.";
    }

    m_running = true;
    emit runningModuleChanged();

    /**
     * Worker thread lambda — OWNs the module lifecycle.
     *
     * This is the ONLY thread where GLFW/OpenGL operations are allowed.
     * The module is created here, run here, and crucially DESTROYED here
     * (after run() returns), ensuring glfwDestroyWindow() and
     * glfwTerminate() execute on the same thread that called glfwInit().
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

            // Prepare Window — show it and attach to the desktop WorkerW layer.
            SetWindowLongPtrW(m_module->hwnd(), GWL_STYLE, WS_VISIBLE);
            wallpaper::desktop::AttachWindowToDesktop(m_module->hwnd());

            // Blocking call: module runs until stop() is called from the main thread.
            m_module->run();

            // Destroy the module on the WORKER thread.
            // This is critical: ~Application() calls glDelete*, glfwDestroyWindow(),
            // and glfwTerminate(), all of which require the GLFW thread's context.
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
}

void WallpaperController::stopWallpaper() {
    if (!m_running) return;

    // 1. Detach and Stop the module instance
    if (m_module) {
        // Try to hide it immediately if it's still alive
        if (IsWindow(m_module->hwnd())) {
            SetWindowLongPtrW(m_module->hwnd(), GWL_STYLE, ~WS_VISIBLE);
            wallpaper::desktop::DetachWindowFromDesktop(m_module->hwnd());
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
    emit runningModuleChanged();

    // 4. Restore the Windows Desktop wallpaper
    restoreWallpaper();

    qInfo() << "Wallpaper stopped and original restored.";
}

void WallpaperController::restoreWallpaper() const {
    wallpaper::desktop::SetWallpaper(m_originalWallpaper);
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
