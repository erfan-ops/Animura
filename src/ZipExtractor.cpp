/**
 * @file ZipExtractor.cpp
 * @brief ZIP inspection and extraction backed by minizip-ng.
 *
 * Uses the backwards-compatible API declared in `unzip.h`:
 *   unzOpen / unzClose
 *   unzLocateFile
 *   unzGoToFirstFile / unzGoToNextFile
 *   unzGetCurrentFileInfo
 *   unzOpenCurrentFile / unzReadCurrentFile / unzCloseCurrentFile
 *
 * All operations are synchronous — no polling, no COM, no shell dependency.
 */

#include "ZipExtractor.hpp"

#include <Windows.h>

#include <minizip-ng/unzip.h>

#include <fstream>

// ── Helpers ──

namespace {

/** RAII wrapper around `unzFile` that calls `unzClose` on scope exit. */
struct UnzFileCloser {
    void operator()(unzFile f) const { if (f) unzClose(f); }
};
using UnzHandle = std::unique_ptr<void, UnzFileCloser>;

/**
 * @brief Opens a ZIP file for reading.
 *
 * `unzOpen` returns NULL on failure (file not found, not a valid ZIP, etc.).
 */
UnzHandle openZip(const std::filesystem::path& zipPath, std::string& outError) {
    // unzOpen takes a const char*; on Windows with the MSVC CRT,
    // the narrow-char path functions accept UTF-8 if the locale is
    // set correctly or via the CRT's _wfopen fallback inside minizip.
    // Convert the wide path to UTF-8 for safe cross-platform usage.
    std::wstring wpath = zipPath.wstring();
    int len = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1,
                                   nullptr, 0, nullptr, nullptr);
    std::string utf8Path(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1,
                         &utf8Path[0], len, nullptr, nullptr);

    unzFile handle = unzOpen(utf8Path.c_str());
    if (!handle) {
        outError = "Failed to open ZIP file: " + zipPath.string();
    }
    return UnzHandle(handle);
}

/**
 * @brief Opens the current entry for reading and copies its decompressed
 *        contents to a filesystem output file.
 *
 * Uses the RAII pattern for the entry — `unzCloseCurrentFile` is called
 * on scope exit even if the write fails.
 *
 * @return true if the entire entry was read and written successfully.
 */
bool extractCurrentEntry(unzFile handle,
                          const std::filesystem::path& destPath,
                          std::string& outError) {
    if (unzOpenCurrentFile(handle) != UNZ_OK) {
        outError = "Failed to open entry for reading.";
        return false;
    }

    // Ensure the entry is always closed.
    struct EntryCloser {
        unzFile h;
        ~EntryCloser() { if (h) unzCloseCurrentFile(h); }
    } closer{handle};

    // Create the output file.
    std::ofstream outFile(destPath, std::ios::binary);
    if (!outFile.is_open()) {
        outError = "Failed to create output file: " + destPath.string();
        return false;
    }

    // Read and write in 64 KiB chunks.
    constexpr int kBufSize = 65536;
    std::vector<uint8_t> buf(kBufSize);
    int bytesRead = 0;
    while ((bytesRead = unzReadCurrentFile(handle, buf.data(), kBufSize)) > 0) {
        outFile.write(reinterpret_cast<const char*>(buf.data()), bytesRead);
        if (!outFile) {
            outError = "Write error while extracting to: " + destPath.string();
            return false;
        }
    }

    // bytesRead < 0 means a decompression error.
    if (bytesRead < 0) {
        outError = "Decompression error reading ZIP entry (code "
                   + std::to_string(bytesRead) + ").";
        return false;
    }

    return true;
}

} // namespace

// ── Public API ──

bool ZipExtractor::HasEntry(
    const std::filesystem::path& zipPath,
    const std::string& entryName) {

    std::string err;
    auto handle = openZip(zipPath, err);
    if (!handle) return false;

    // unzLocateFile returns UNZ_OK if found, UNZ_END_OF_LIST_OF_FILE if not.
    // The third parameter is case-sensitivity: 0 = OS default, 1 = sensitive.
    return unzLocateFile(handle.get(), entryName.c_str(), 1) == UNZ_OK;
}

std::optional<std::string> ZipExtractor::ReadFile(
    const std::filesystem::path& zipPath,
    const std::string& entryName) {

    std::string err;
    auto handle = openZip(zipPath, err);
    if (!handle) return std::nullopt;

    // Locate the entry by name (case-sensitive).
    if (unzLocateFile(handle.get(), entryName.c_str(), 1) != UNZ_OK) {
        return std::nullopt;
    }

    // Get the uncompressed size so we can pre-allocate.
    unz_file_info64 info;
    if (unzGetCurrentFileInfo64(handle.get(), &info,
                                 nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK) {
        return std::nullopt;
    }

    if (unzOpenCurrentFile(handle.get()) != UNZ_OK) {
        return std::nullopt;
    }

    struct EntryCloser {
        unzFile h;
        ~EntryCloser() { if (h) unzCloseCurrentFile(h); }
    } closer{handle.get()};

    // Pre-allocate based on uncompressed size (capped for safety).
    const size_t cap = static_cast<size_t>(
        std::min<int64_t>(info.uncompressed_size, 16 * 1024 * 1024)); // 16 MiB cap
    std::string result;
    result.reserve(cap > 0 ? cap : 4096);

    constexpr int kBufSize = 16384;
    char buf[kBufSize];
    int bytesRead = 0;
    while ((bytesRead = unzReadCurrentFile(handle.get(), buf, kBufSize)) > 0) {
        result.append(buf, static_cast<size_t>(bytesRead));
    }

    if (bytesRead < 0) {
        return std::nullopt; // Decompression error.
    }

    return result;
}

bool ZipExtractor::isSafeEntryPath(const std::string& entryName) {
    // Reject empty names.
    if (entryName.empty()) return false;

    // Reject absolute paths.
    if (entryName.size() >= 1 && (entryName[0] == '/' || entryName[0] == '\\')) {
        return false;
    }
    // Windows absolute: "C:\..." or "C:/..."
    if (entryName.size() >= 3 && entryName[1] == ':'
        && (entryName[2] == '/' || entryName[2] == '\\')) {
        return false;
    }

    // Reject paths containing ".." (parent-directory traversal).
    // Check each component.
    for (size_t i = 0; i + 1 < entryName.size(); ++i) {
        if (entryName[i] == '.' && entryName[i + 1] == '.') {
            // Must be a path component (preceded by separator or at start,
            // followed by separator or at end).
            bool atStart = (i == 0);
            bool prevSep = (i > 0 && (entryName[i - 1] == '/' || entryName[i - 1] == '\\'));
            bool nextSep  = (i + 2 >= entryName.size())
                         || (entryName[i + 2] == '/' || entryName[i + 2] == '\\');
            if ((atStart || prevSep) && nextSep) {
                return false;
            }
        }
    }

    return true;
}

bool ZipExtractor::ExtractAll(
    const std::filesystem::path& zipPath,
    const std::filesystem::path& destDir,
    std::string& outError) {

    // Create the destination directory (and parents).
    std::error_code ec;
    std::filesystem::create_directories(destDir, ec);
    if (ec) {
        outError = "Failed to create destination directory: "
                   + destDir.string() + " (" + ec.message() + ")";
        return false;
    }

    auto handle = openZip(zipPath, outError);
    if (!handle) return false;

    // Iterate all entries.
    if (unzGoToFirstFile(handle.get()) != UNZ_OK) {
        outError = "ZIP archive contains no entries.";
        return false;
    }

    do {
        // Get the entry's filename.
        unz_file_info64 info;
        char entryName[1024];
        if (unzGetCurrentFileInfo64(handle.get(), &info,
                                     entryName, sizeof(entryName),
                                     nullptr, 0, nullptr, 0) != UNZ_OK) {
            outError = "Failed to read ZIP entry info.";
            return false;
        }

        std::string name(entryName);

        // ── Path-traversal guard ──
        if (!isSafeEntryPath(name)) {
            outError = "Rejected unsafe ZIP entry path: \"" + name + "\".";
            return false;
        }

        // Skip directory entries (trailing slash, or marked as dir).
        if (!name.empty() && (name.back() == '/' || name.back() == '\\')) {
            continue;
        }
        if (info.uncompressed_size == 0 && info.compressed_size == 0
            && info.size_filename > 0) {
            // Heuristic: zero-size entry with a name — likely a directory marker.
            continue;
        }

        // Build the destination path.
        std::filesystem::path destPath = destDir / name;

        // Ensure the parent directory exists (for entries in subdirs).
        if (destPath.has_parent_path()) {
            std::filesystem::create_directories(destPath.parent_path(), ec);
            if (ec) {
                outError = "Failed to create directory: "
                           + destPath.parent_path().string();
                return false;
            }
        }

        if (!extractCurrentEntry(handle.get(), destPath, outError)) {
            return false;
        }
    } while (unzGoToNextFile(handle.get()) == UNZ_OK);

    return true;
}

bool ZipExtractor::ListRootEntries(
    const std::filesystem::path& zipPath,
    std::vector<std::string>& entries,
    std::string& outError) {

    entries.clear();

    auto handle = openZip(zipPath, outError);
    if (!handle) return false;

    if (unzGoToFirstFile(handle.get()) != UNZ_OK) {
        // Empty archive — not an error, just no entries.
        return true;
    }

    do {
        char entryName[1024];
        unz_file_info64 info;
        if (unzGetCurrentFileInfo64(handle.get(), &info,
                                     entryName, sizeof(entryName),
                                     nullptr, 0, nullptr, 0) != UNZ_OK) {
            continue; // Skip malformed entries.
        }

        std::string name(entryName);

        // Root entry: no '/' or '\' in the name.
        if (name.find('/') == std::string::npos
            && name.find('\\') == std::string::npos) {
            entries.push_back(name);
        }
    } while (unzGoToNextFile(handle.get()) == UNZ_OK);

    return true;
}
