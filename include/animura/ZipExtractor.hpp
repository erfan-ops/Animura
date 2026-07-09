#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief ZIP archive inspection and extraction backed by minizip-ng.
 *
 * All methods are synchronous and deterministic — no polling, no COM,
 * no shell dependency. The implementation uses the minizip-ng
 * compatibility API (`unz*` functions from `unzip.h`).
 *
 * ## Thread Safety
 * All methods perform file I/O and should be called from the Qt main
 * thread (or any thread with a message pump, to keep UI responsive).
 */
class ZipExtractor {
public:
    /**
     * @brief Checks whether a named entry exists in the ZIP.
     *
     * Uses `unzLocateFile` — a single seek inside the central directory.
     * No decompression is performed.
     *
     * @param zipPath   Path to the ZIP file.
     * @param entryName Name of the entry to look for (e.g. "module.json").
     * @return true if the entry exists.
     */
    static bool HasEntry(
        const std::filesystem::path& zipPath,
        const std::string& entryName);

    /**
     * @brief Reads a single file from the ZIP into memory.
     *
     * Opens the ZIP, locates the entry, decompresses its contents, and
     * returns the result as a string. Suitable for small text files like
     * `module.json`.
     *
     * @param zipPath   Path to the ZIP file.
     * @param entryName Name of the entry to read.
     * @return The file contents, or `std::nullopt` on failure.
     */
    static std::optional<std::string> ReadFile(
        const std::filesystem::path& zipPath,
        const std::string& entryName);

    /**
     * @brief Extracts the entire ZIP archive to a directory.
     *
     * Creates the destination directory if needed. Every entry is
     * validated for path traversal before extraction. Entries with
     * dangerous paths (absolute, parent-directory references) are
     * rejected.
     *
     * Extraction is fully synchronous — the call returns only after
     * all files have been written and the archive handle is closed.
     *
     * On failure the partially-created destination directory is NOT
     * automatically removed; the caller is responsible for cleanup.
     *
     * @param zipPath  Path to the ZIP file.
     * @param destDir  Destination directory (created if absent).
     * @param outError Receives a description of any error.
     * @return true if all entries were extracted successfully.
     */
    static bool ExtractAll(
        const std::filesystem::path& zipPath,
        const std::filesystem::path& destDir,
        std::string& outError);

    /**
     * @brief Returns the names of all root-level entries in the ZIP.
     *
     * "Root-level" means the entry name contains no directory separator
     * ('/' or '\\'). Directory entries (trailing slash) and entries in
     * subdirectories are excluded.
     *
     * @param zipPath  Path to the ZIP file.
     * @param entries  Output — receives root-level entry names.
     * @param outError Receives a description of any error.
     * @return true on success.
     */
    static bool ListRootEntries(
        const std::filesystem::path& zipPath,
        std::vector<std::string>& entries,
        std::string& outError);

private:
    ZipExtractor() = delete;

    /**
     * @brief Checks whether a ZIP entry path is safe to extract.
     *
     * Rejects:
     * - Absolute paths (e.g. `C:\...` on Windows, `/etc/...` on POSIX)
     * - Paths containing `..` (parent-directory traversal)
     *
     * @param entryName The entry filename from the ZIP central directory.
     * @return true if the path is safe.
     */
    static bool isSafeEntryPath(const std::string& entryName);
};
