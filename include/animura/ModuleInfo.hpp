#pragma once
#include <string>
#include <filesystem>
#include <QString>

struct ModuleInfo {
    std::filesystem::path basePath;

    std::string name;
    std::string version;

    std::filesystem::path entryDll;
    std::filesystem::path schemaFile;
    std::filesystem::path settingsFile;
    std::filesystem::path previewFile;

    std::filesystem::path entryPath() const { return basePath / entryDll; }
    std::filesystem::path schemaPath() const { return basePath / schemaFile; }
    std::filesystem::path settingsPath() const { return basePath / settingsFile; }
    std::filesystem::path previewPathFs() const { return basePath / previewFile; }
    QString previewPathQt() const {
        // Return a virtual-host URL that WebView2 resolves to the modules folder.
        return QStringLiteral("https://animura.modules/%1/%2")
            .arg(QString::fromStdWString(basePath.filename().wstring()),
                 QString::fromStdWString(previewFile.wstring()));
    }
};
