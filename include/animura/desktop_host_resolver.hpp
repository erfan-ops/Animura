#pragma once

#include <Windows.h>

namespace animura::desktop {

// ------------------------------------------------------------------
// Desktop shell layout variants
// ------------------------------------------------------------------
enum class DesktopLayout {
    Classic,           // WorkerW (top-level) -> SHELLDLL_DefView
    ModernLayered,     // Progman -> SHELLDLL_DefView, no WorkerW child
    ModernRaised,      // Progman -> SHELLDLL_DefView + WorkerW sibling child
    Unknown
};

// ------------------------------------------------------------------
// Immutable result of host resolution
// ------------------------------------------------------------------
struct DesktopHostInfo {
    HWND hwndHost{nullptr};      // The window to SetParent() to
    HWND hwndDefView{nullptr};   // SHELLDLL_DefView handle
    HWND hwndWorkerW{nullptr};   // WorkerW if applicable (for Z-order)
    HWND hwndProgman{nullptr};   // Progman handle
    DesktopLayout layout{DesktopLayout::Unknown};

    [[nodiscard]] bool IsValid() const noexcept {
        return hwndHost != nullptr && hwndDefView != nullptr;
    }
};

// ------------------------------------------------------------------
// Probes the Windows Shell to find the correct wallpaper host.
// ------------------------------------------------------------------
class DesktopHostResolver {
public:
    DesktopHostResolver() = default;
    ~DesktopHostResolver() = default;

    DesktopHostResolver(const DesktopHostResolver&) = delete;
    DesktopHostResolver& operator=(const DesktopHostResolver&) = delete;
    DesktopHostResolver(DesktopHostResolver&&) noexcept = default;
    DesktopHostResolver& operator=(DesktopHostResolver&&) noexcept = default;

    // Main entry point. Thread-safe regarding Win32 window operations.
    [[nodiscard]] DesktopHostInfo Resolve();

private:
    void EnsureShellWorkerInitialized(HWND hwndProgman);

    struct DefViewSearchResult {
        HWND defView{nullptr};
        HWND parent{nullptr};
    };
    [[nodiscard]] DefViewSearchResult FindDefViewAndParent();

    [[nodiscard]] DesktopLayout ClassifyLayout(HWND hwndProgman,
                                               HWND hwndParent,
                                               HWND hwndDefView);
};

} // namespace animura::desktop