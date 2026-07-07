#pragma once
#include <windows.h>
#include <string>

#if defined(WALLPAPER_HOST_STATIC)
#  define WALLPAPER_HOST_API
#elif defined(BUILDING_WALLPAPER_HOST)
#  define WALLPAPER_HOST_API __declspec(dllexport)
#else
#  define WALLPAPER_HOST_API __declspec(dllimport)
#endif

namespace wallpaper {
namespace desktop {

/**
 * Initializes internal function pointers for wallpaper operations.
 *
 * Must be called before any invocation of SetWallpaper or GetCurrentWallpaperPath.
 * Automatically selects the COM-based or registry-based implementation depending
 * on availability.
 */
WALLPAPER_HOST_API void initSetWallpaperMethod();

WALLPAPER_HOST_API void shutdownWallpaperMethod();

/**
 * Returns the absolute path of the current desktop wallpaper.
 *
 * The returned std::wstring is owned by the caller.
 *
 * @return absolute path (possibly empty if retrieval failed).
 */
WALLPAPER_HOST_API std::wstring GetCurrentWallpaperPath();

/**
 * sets the current desktop wallpaper.
 *
 * @param path absolute path of an image.
 * @return true on success.
 */
WALLPAPER_HOST_API bool SetWallpaper(std::wstring path);

/**
 * Attaches the specified top-level window (hwnd) to the Windows desktop by
 * parenting it to the WorkerW window used for the wallpaper. On success, the
 * window behaves like a wallpaper window (behind icons).
 *
 * Implementation notes:
 *  - Uses the Progman message trick to force creation of a WorkerW and then
 *    searches for the WorkerW sibling which contains the SHELLDLL_DefView.
 *
 * @param hwnd Window to attach (should be a top-level, visible window).
 * @return 0 on success, 1 when hwnd is not a window, 2 when no reliable WorkerW is found.
 */
WALLPAPER_HOST_API int AttachWindowToDesktop(HWND hwnd);

/**
 * Attempts to restore the window to a top-level (no parent) state. Call when tearing down.
 *
 * @param hwnd Window previously attached.
 * @return true on success.
 */
WALLPAPER_HOST_API bool DetachWindowFromDesktop(HWND hwnd);

} // namespace desktop
} // namespace wallpaper
