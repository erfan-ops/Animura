#pragma once

#include <QObject>
#include <QJsonObject>

#include <thread>
#include <atomic>
#include <memory>

#include "ModuleCatalog.hpp"
#include "ModuleLibrary.hpp"
#include "IWallpaperModule.hpp"

class WallpaperController : public QObject {
    Q_OBJECT

public:
    explicit WallpaperController(QObject* parent = nullptr);
    ~WallpaperController();

    Q_INVOKABLE QVariantList getModulesList() const;
    Q_INVOKABLE int getRunningModuleId() const;
    Q_INVOKABLE void startWallpaper(int moduleIndex);
    Q_INVOKABLE void stopWallpaper();
    Q_INVOKABLE QJsonObject loadSettingsUI(int moduleIndex);
    Q_INVOKABLE void applySettings(int moduleIndex, QJsonObject settings);

signals:
    void runningModuleChanged();

private:
    ModuleCatalog m_catalog;
    ModuleLibrary m_library;

    std::unique_ptr<IWallpaperModule> m_module;
    std::thread m_worker;
    std::atomic_bool m_running{ false };

    int m_currentModuleId{ -1 };
    std::wstring m_originalWallpaper;

    void restoreWallpaper() const;
    bool validateIndex(int moduleIndex) const;
    bool validateModuleFiles(const ModuleInfo& info) const;
    bool validateModuleSettings(const ModuleInfo& info) const;
    bool ensureLibraryLoaded(const ModuleInfo& info, int moduleIndex);
    void startWorker(const ModuleInfo& info);
};
