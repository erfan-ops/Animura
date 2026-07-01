#include "NativeBridge.hpp"
#include "WallpaperController.hpp"

#include <QMetaObject>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <QDebug>
#include <sstream>

// ── DISPID constants (method IDs for Invoke) ──
enum BridgeDispId {
    DISPID_GET_MODULES_LIST     = 1,
    DISPID_GET_RUNNING_MODULE_ID = 2,
    DISPID_START_WALLPAPER      = 3,
    DISPID_STOP_WALLPAPER       = 4,
    DISPID_LOAD_SETTINGS_UI     = 5,
    DISPID_APPLY_SETTINGS       = 6,
    DISPID_GET_ACCENT_COLOR     = 7,
};

// ── Name → DISPID map ──
static const std::pair<const wchar_t*, DISPID> kMethodMap[] = {
    { L"GetModulesList",     DISPID_GET_MODULES_LIST },
    { L"GetRunningModuleId", DISPID_GET_RUNNING_MODULE_ID },
    { L"StartWallpaper",     DISPID_START_WALLPAPER },
    { L"StopWallpaper",      DISPID_STOP_WALLPAPER },
    { L"LoadSettingsUI",     DISPID_LOAD_SETTINGS_UI },
    { L"ApplySettings",      DISPID_APPLY_SETTINGS },
    { L"GetAccentColor",     DISPID_GET_ACCENT_COLOR },
};

// ── Helper: convert QJsonObject to std::string ──
static std::string qjsonToString(const QJsonObject& obj) {
    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact).toStdString();
}

static std::string qvariantListToString(const QVariantList& list) {
    // ModuleInfo::previewPathQt() now returns https://animura.modules/… URLs
    // directly, so no path conversion is needed here.
    QJsonDocument doc(QJsonArray::fromVariantList(list));
    return doc.toJson(QJsonDocument::Compact).toStdString();
}

// ── Constructor ──

NativeBridge::NativeBridge(WallpaperController* controller)
    : m_controller(controller) {
}

// ── IUnknown ──

HRESULT NativeBridge::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDispatch) {
        *ppv = static_cast<IDispatch*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

ULONG NativeBridge::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

ULONG NativeBridge::Release() {
    LONG count = InterlockedDecrement(&m_refCount);
    if (count == 0) delete this;
    return count;
}

// ── IDispatch ──

HRESULT NativeBridge::GetTypeInfoCount(UINT* pctinfo) {
    *pctinfo = 0;
    return S_OK;
}

HRESULT NativeBridge::GetTypeInfo(UINT, LCID, ITypeInfo**) {
    return E_NOTIMPL;
}

HRESULT NativeBridge::GetIDsOfNames(
    REFIID, LPOLESTR* rgszNames, UINT cNames, LCID, DISPID* rgDispId) {
    if (cNames != 1 || !rgszNames || !rgDispId) return E_INVALIDARG;

    for (const auto& [name, id] : kMethodMap) {
        if (wcscmp(rgszNames[0], name) == 0) {
            *rgDispId = id;
            return S_OK;
        }
    }
    return DISP_E_UNKNOWNNAME;
}

HRESULT NativeBridge::Invoke(
    DISPID dispIdMember, REFIID, LCID, WORD wFlags,
    DISPPARAMS* pDispParams, VARIANT* pVarResult,
    EXCEPINFO*, UINT*) {

    if (!(wFlags & DISPATCH_METHOD)) return DISP_E_MEMBERNOTFOUND;

    HRESULT hr = S_OK;
    std::string result;

    // Marshal to Qt main thread. Use BlockingQueuedConnection for cross-thread
    // calls (from WebView2 thread), or execute directly if on main thread.
    Qt::ConnectionType connType = Qt::BlockingQueuedConnection;
    if (m_controller->thread() == QThread::currentThread()) {
        connType = Qt::DirectConnection;
    }

    QMetaObject::invokeMethod(m_controller, [&]() {
        switch (dispIdMember) {
            case DISPID_GET_MODULES_LIST:
                result = qvariantListToString(m_controller->getModulesList());
                break;

            case DISPID_GET_RUNNING_MODULE_ID:
                result = std::to_string(m_controller->getRunningModuleId());
                break;

            case DISPID_START_WALLPAPER: {
                int idx = 0;
                if (pDispParams->cArgs >= 1) {
                    VariantChangeType(&pDispParams->rgvarg[0],
                        &pDispParams->rgvarg[0], 0, VT_I4);
                    idx = pDispParams->rgvarg[0].intVal;
                }
                m_controller->startWallpaper(idx);
                break;
            }

            case DISPID_STOP_WALLPAPER:
                m_controller->stopWallpaper();
                break;

            case DISPID_LOAD_SETTINGS_UI: {
                int idx = 0;
                if (pDispParams->cArgs >= 1) {
                    VariantChangeType(&pDispParams->rgvarg[0],
                        &pDispParams->rgvarg[0], 0, VT_I4);
                    idx = pDispParams->rgvarg[0].intVal;
                }
                result = qjsonToString(m_controller->loadSettingsUI(idx));
                break;
            }

            case DISPID_APPLY_SETTINGS: {
                int idx = 0;
                std::string jsonStr;
                if (pDispParams->cArgs >= 2) {
                    VariantChangeType(&pDispParams->rgvarg[1],
                        &pDispParams->rgvarg[1], 0, VT_I4);
                    idx = pDispParams->rgvarg[1].intVal;

                    if (pDispParams->rgvarg[0].vt == VT_BSTR) {
                        std::wstring ws(pDispParams->rgvarg[0].bstrVal);
                        jsonStr = std::string(ws.begin(), ws.end());
                    }
                }
                QJsonDocument doc = QJsonDocument::fromJson(
                    QString::fromStdString(jsonStr).toUtf8());
                m_controller->applySettings(idx, doc.object());
                break;
            }

            case DISPID_GET_ACCENT_COLOR:
                result = "#e04090";
                break;

            default:
                hr = DISP_E_MEMBERNOTFOUND;
                break;
        }
    }, connType);

    if (pVarResult && !result.empty()) {
        V_VT(pVarResult) = VT_BSTR;
        std::wstring wresult(result.begin(), result.end());
        pVarResult->bstrVal = SysAllocString(wresult.c_str());
    } else if (pVarResult) {
        V_VT(pVarResult) = VT_EMPTY;
    }

    return hr;
}

void NativeBridge::setMessageCallback(MessageCallback callback) {
    m_callback = std::move(callback);
}
