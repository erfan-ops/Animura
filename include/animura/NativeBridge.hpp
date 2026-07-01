#pragma once

#include <string>
#include <functional>
#include <ocidl.h>

class WallpaperController;

/**
 * COM IDispatch host object exposed to JavaScript via WebView2's AddHostObjectToScript.
 * Provides typed bridge between React frontend and C++ backend.
 *
 * Thread safety: All methods marshal to the Qt main thread via QMetaObject::invokeMethod
 * before accessing WallpaperController.
 */
class NativeBridge : public IDispatch {
public:
    explicit NativeBridge(WallpaperController* controller);
    virtual ~NativeBridge() = default;

    // ── IUnknown ──
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override;
    ULONG   STDMETHODCALLTYPE AddRef() override;
    ULONG   STDMETHODCALLTYPE Release() override;

    // ── IDispatch ──
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* pctinfo) override;
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) override;
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(
        REFIID riid, LPOLESTR* rgszNames, UINT cNames,
        LCID lcid, DISPID* rgDispId) override;
    HRESULT STDMETHODCALLTYPE Invoke(
        DISPID dispIdMember, REFIID riid, LCID lcid,
        WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarResult,
        EXCEPINFO* pExcepInfo, UINT* puArgErr) override;

    using MessageCallback = std::function<void(const std::string& json)>;
    void setMessageCallback(MessageCallback callback);

private:
    WallpaperController* m_controller;
    MessageCallback m_callback;
    LONG m_refCount{ 1 };
};
