#pragma once

#include <Windows.h>
#include <string>
#include <utility>
#include <memory>

namespace animura::win32 {

// ------------------------------------------------------------------
// RAII wrapper for COM apartment initialization
// ------------------------------------------------------------------
class ScopedCoInit {
public:
    explicit ScopedCoInit(DWORD apartment = COINIT_APARTMENTTHREADED);
    ~ScopedCoInit();
    ScopedCoInit(const ScopedCoInit&) = delete;
    ScopedCoInit& operator=(const ScopedCoInit&) = delete;
    ScopedCoInit(ScopedCoInit&&) = delete;
    ScopedCoInit& operator=(ScopedCoInit&&) = delete;
private:
    HRESULT m_hr;
};

// ------------------------------------------------------------------
// Kernel object RAII (events, mutexes, etc.)
// ------------------------------------------------------------------
struct HandleDeleter {
    using pointer = HANDLE;
    void operator()(HANDLE h) const noexcept {
        if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
};
using UniqueHandle = std::unique_ptr<HANDLE, HandleDeleter>;

// ------------------------------------------------------------------
// Window style helpers
// ------------------------------------------------------------------
[[nodiscard]] LONG_PTR GetWindowStyle(HWND hwnd);
[[nodiscard]] LONG_PTR GetWindowExStyle(HWND hwnd);
[[nodiscard]] bool HasWindowStyle(HWND hwnd, LONG_PTR style);
[[nodiscard]] bool HasExtendedStyle(HWND hwnd, LONG_PTR style);

// Strip decorative styles and enforce child/popup cleanliness.
void SanitizeWindowStyles(HWND hwnd, bool makeChild);

// ------------------------------------------------------------------
// Display geometry
// ------------------------------------------------------------------
struct VirtualScreenGeometry {
    int x{0}, y{0};
    int width{0}, height{0};
};
[[nodiscard]] VirtualScreenGeometry GetVirtualScreenGeometry();

// ------------------------------------------------------------------
// Error formatting
// ------------------------------------------------------------------
[[nodiscard]] std::wstring GetLastErrorString();

} // namespace animura::win32