#pragma once

#include <string>
#include <functional>
#include <ocidl.h>

class WallpaperController;
class QWidget;

/**
 * @brief COM IDispatch host object exposed to JavaScript via WebView2's
 *        `AddHostObjectToScript`.
 *
 * NativeBridge provides the typed bridge between the React frontend (running
 * in WebView2) and the C++ backend (WallpaperController). WebView2 calls
 * `Invoke()` on arbitrary COM threads; all method implementations marshal to
 * the Qt main thread via `QMetaObject::invokeMethod` before accessing
 * WallpaperController state.
 *
 * ## DISPID Method Map
 * | DISPID | JS Method            | C++ Target                 |
 * |--------|----------------------|----------------------------|
 * | 1      | `GetModulesList`     | `getModulesList()`         |
 * | 2      | `GetRunningModuleId` | `getRunningModuleId()`     |
 * | 3      | `StartWallpaper(idx)`| `startWallpaper(int)`      |
 * | 4      | `StopWallpaper`      | `stopWallpaper()`          |
 * | 5      | `LoadSettingsUI(idx)`| `loadSettingsUI(int)`      |
 * | 6      | `ApplySettings(i, j)`| `applySettings(int, json)` |
 * | 7      | `PickFile(filter?)`   | Opens native file dialog, returns path |
 *
 * ## Thread Safety
 * - WebView2 calls `Invoke()` on arbitrary threads.
 * - All methods are marshaled to the Qt main thread.
 * - `Qt::BlockingQueuedConnection` is used for cross-thread calls (blocks
 *   the WebView2 thread until the main thread processes the call).
 * - `Qt::DirectConnection` is used when already on the main thread to
 *   avoid deadlock.
 *
 * ## COM Return Value Convention
 * All methods return `BSTR` (COM string). The JS bridge wrapper in
 * `frontend/src/bridge/native.ts` handles type coercion — for example,
 * `getRunningModuleId()` returns a string like `"0"` which is parsed
 * with `Number()`.
 *
 * @see WallpaperController for the backend methods this bridge delegates to.
 * @see frontend/src/bridge/native.ts for the typed JS wrapper.
 */
class NativeBridge : public IDispatch {
public:
    /**
     * @brief Constructs the bridge with a reference to the controller and
     *        the main window widget (used as parent for native dialogs).
     * @param controller The WallpaperController to delegate calls to.
     *                   Non-owning pointer — must outlive this bridge.
     * @param parentWindow The top-level QWidget window, used as the parent
     *                     for native dialogs like QFileDialog.
     */
    explicit NativeBridge(WallpaperController* controller, QWidget* parentWindow = nullptr);

    /** Virtual destructor for COM interface safety. */
    virtual ~NativeBridge() = default;

    // ── IUnknown ──

    /**
     * @brief COM QueryInterface — supports IUnknown and IDispatch.
     * @return S_OK if the requested interface is supported, E_NOINTERFACE otherwise.
     */
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override;

    /**
     * @brief Increments the COM reference count (thread-safe via InterlockedIncrement).
     * @return The new reference count.
     */
    ULONG   STDMETHODCALLTYPE AddRef() override;

    /**
     * @brief Decrements the COM reference count. Deletes `this` when count reaches zero.
     * @return The new reference count.
     */
    ULONG   STDMETHODCALLTYPE Release() override;

    // ── IDispatch ──

    /** Returns 0 (no type info provided). */
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* pctinfo) override;

    /** Not implemented — returns E_NOTIMPL. */
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) override;

    /**
     * @brief Resolves a method name to its DISPID.
     *
     * Looks up the name in the static `kMethodMap` table. Only supports
     * single-name lookups (`cNames == 1`).
     *
     * @return S_OK if the name is found, DISP_E_UNKNOWNNAME otherwise.
     */
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(
        REFIID riid, LPOLESTR* rgszNames, UINT cNames,
        LCID lcid, DISPID* rgDispId) override;

    /**
     * @brief Executes a method identified by its DISPID.
     *
     * Dispatches the call to the appropriate WallpaperController method via
     * `QMetaObject::invokeMethod` to marshal to the Qt main thread. The
     * WebView2 calling thread is blocked until the main thread completes
     * (BlockingQueuedConnection) unless already on the main thread
     * (DirectConnection).
     *
     * Extracts arguments from `pDispParams` using COM VARIANT conversions,
     * and returns results as BSTR in `pVarResult`.
     *
     * @param dispIdMember The DISPID resolved by GetIDsOfNames.
     * @param wFlags       Must contain DISPATCH_METHOD.
     * @param pDispParams  Method arguments (reversed order per COM convention).
     * @param pVarResult   Output — receives the return value as BSTR.
     * @return S_OK on success, DISP_E_MEMBERNOTFOUND if DISPID is unknown.
     */
    HRESULT STDMETHODCALLTYPE Invoke(
        DISPID dispIdMember, REFIID riid, LCID lcid,
        WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarResult,
        EXCEPINFO* pExcepInfo, UINT* puArgErr) override;

    /**
     * @brief Sets an optional callback for messages received from JavaScript.
     *
     * Currently unused in production — the primary C++→JS path is
     * `WebView2Host::postMessageToJs()`.
     */
    using MessageCallback = std::function<void(const std::string& json)>;
    void setMessageCallback(MessageCallback callback);

private:
    /** The backend controller. Non-owning — owned by main(). */
    WallpaperController* m_controller;

    /**
     * The main application window. Used as the parent for native dialogs
     * (e.g., QFileDialog) to ensure correct window stacking and focus.
     * Non-owning — owned by WebView2Host. May be nullptr.
     */
    QWidget* m_parentWindow{ nullptr };

    /** Optional callback for JS→C++ web messages. Currently unused. */
    MessageCallback m_callback;

    /** COM reference count. Initialized to 1. Managed by AddRef/Release. */
    LONG m_refCount{ 1 };
};
