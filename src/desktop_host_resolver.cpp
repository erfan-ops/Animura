#include "animura/desktop_host_resolver.hpp"
#include "animura/win32_utils.hpp"

#include <string>

namespace animura::desktop {

// Undocumented message to trigger shell worker (WorkerW) creation.
constexpr UINT kShellWorkerMessage = 0x052C;

// ------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------
DesktopHostInfo DesktopHostResolver::Resolve() {
    DesktopHostInfo info{};
    info.hwndProgman = FindWindowW(L"Progman", nullptr);
    if (!info.hwndProgman) {
        return info;
    }

    // Step 1: Force the shell to initialize the desktop worker infrastructure.
    // This is required on classic layouts where WorkerW may not exist until
    // we poke Progman. On modern layouts this is effectively a no-op but harmless.
    EnsureShellWorkerInitialized(info.hwndProgman);

    // Step 2: Locate SHELLDLL_DefView and its immediate parent.
    // We enumerate top-level windows because DefView may live under Progman
    // (modern) or under a top-level WorkerW (classic).
    auto result = FindDefViewAndParent();

    // Fallback: direct search if enumeration failed (rare).
    if (!result.defView) {
        result.defView = FindWindowW(L"SHELLDLL_DefView", nullptr);
        if (result.defView) {
            result.parent = GetParent(result.defView);
        }
    }

    if (!result.defView || !result.parent) {
        return info;
    }

    info.hwndDefView = result.defView;
    info.layout = ClassifyLayout(info.hwndProgman, result.parent, result.defView);

    // If we are in the modern raised state, Progman hosts both DefView and a
    // WorkerW child. We need the WorkerW handle to sandwich our Z-order.
    if (info.layout == DesktopLayout::ModernRaised) {
        info.hwndWorkerW = FindWindowExW(info.hwndProgman, nullptr, L"WorkerW", nullptr);
    } else if (info.layout == DesktopLayout::Classic) {
        info.hwndWorkerW = FindWindowExW(nullptr, result.parent, L"WorkerW", nullptr);
    }

    info.hwndHost = info.hwndWorkerW;

    return info;
}

// ------------------------------------------------------------------
// Step 1: Trigger shell worker creation
// ------------------------------------------------------------------
void DesktopHostResolver::EnsureShellWorkerInitialized(HWND hwndProgman) {
    // Lively and other engines send 0x052C with wParam=0xD.
    // lParam varies by Windows build (0 or 1). We send both to be thorough.
    DWORD_PTR result = 0;
    SendMessageTimeoutW(hwndProgman, kShellWorkerMessage, 0xD, 0,
                        SMTO_NORMAL, 1000, &result);
    SendMessageTimeoutW(hwndProgman, kShellWorkerMessage, 0xD, 1,
                        SMTO_NORMAL, 1000, &result);
}

// ------------------------------------------------------------------
// Step 2: Parent-of-SHELLDLL_DefView lookup
// ------------------------------------------------------------------
DesktopHostResolver::DefViewSearchResult
DesktopHostResolver::FindDefViewAndParent() {
    DefViewSearchResult result{};

    EnumWindows(
        [](HWND hwndTopLevel, LPARAM lParam) -> BOOL {
            auto* out = reinterpret_cast<DefViewSearchResult*>(lParam);

            // Is SHELLDLL_DefView an immediate child of this top-level window?
            HWND child = FindWindowExW(hwndTopLevel, nullptr,
                                       L"SHELLDLL_DefView", nullptr);
            if (child) {
                out->defView = child;
                out->parent = hwndTopLevel;
                return FALSE; // Stop: we found the desktop's DefView.
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&result));

    return result;
}

// ------------------------------------------------------------------
// Step 3: Classify layout based on parent identity & Progman styles
// ------------------------------------------------------------------
DesktopLayout DesktopHostResolver::ClassifyLayout(HWND hwndProgman,
                                                  HWND hwndParent,
                                                  HWND /*hwndDefView*/) {
    const bool isModern =
        win32::HasExtendedStyle(hwndProgman, WS_EX_NOREDIRECTIONBITMAP);

    if (hwndParent == hwndProgman) {
        // DefView lives directly under Progman. Determine if raised.
        HWND workerUnderProgman =
            FindWindowExW(hwndProgman, nullptr, L"WorkerW", nullptr);
        return (workerUnderProgman != nullptr)
                   ? DesktopLayout::ModernRaised
                   : DesktopLayout::ModernLayered;
    }

    // DefView lives under a WorkerW (top-level or nested). This is the classic
    // layout that has worked since Windows 7.
    return DesktopLayout::Classic;
}

} // namespace animura::desktop