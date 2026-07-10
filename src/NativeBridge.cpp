/**
 * @file NativeBridge.cpp
 * @brief Implementation of the COM IDispatch bridge between the React
 *        frontend (JavaScript) and the C++ backend (WallpaperController).
 *
 * NativeBridge is registered with WebView2 via AddHostObjectToScript,
 * making it available to JS as `window.chrome.webview.hostObjects.nativeBridge`.
 *
 * ## DISPID Method Map
 * | DISPID | JS Method            | C++ Target                 |
 * |--------|----------------------|----------------------------|
 * | 1      | `GetModulesList`     | `getModulesList()`         |
 * | 2      | `GetRunningModuleId` | `getRunningModuleId()`     |
 * | 3      | `StartWallpaper(idx)`| `startWallpaper(int)`      |
 * | 4      | `StopWallpaper`      | `stopWallpaper()`          |
 * | 5      | `LoadSettingsUI(idx)`| `loadSettingsUI(int)`      |
 * | 6      | `ApplySettings(i,j)` | `applySettings(int, json)` |
 * | 7      | `PickFile`           | `pickFile(filter?)`       |
 * | 8      | `InstallModule`      | `installModule()`         |
 *
 * ## Thread Safety
 * WebView2 calls `Invoke()` on arbitrary COM threads. All method
 * implementations marshal to the Qt main thread using
 * `QMetaObject::invokeMethod`. Cross-thread calls use
 * `Qt::BlockingQueuedConnection` (blocks the WebView2 thread until the
 * main thread processes the call). When already on the main thread,
 * `Qt::DirectConnection` is used to avoid self-deadlock.
 *
 * ## Return Value Convention
 * All methods return `BSTR` (COM string). The JS bridge wrapper in
 * `frontend/src/bridge/native.ts` handles type coercion — for example,
 * `getRunningModuleId()` returns a string like `"0"` which must be
 * parsed with `Number()`.
 */

#include "NativeBridge.hpp"
#include "WallpaperController.hpp"

#include <QMetaObject>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <QDebug>
#include <QFileDialog>
#include <QWidget>

#include <sstream>

// ── DISPID constants (method IDs for Invoke) ──
enum BridgeDispId {
    DISPID_GET_MODULES_LIST      = 1,
    DISPID_GET_RUNNING_MODULE_ID = 2,
    DISPID_START_WALLPAPER       = 3,
    DISPID_STOP_WALLPAPER        = 4,
    DISPID_LOAD_SETTINGS_UI      = 5,
    DISPID_APPLY_SETTINGS        = 6,
    DISPID_PICK_FILE             = 7,
    DISPID_INSTALL_MODULE        = 8,
    DISPID_DETACH_WALLPAPER      = 9,
    DISPID_ATTACH_WALLPAPER      = 10,
    DISPID_GET_IS_ATTACHED       = 11,
};

// ── Name → DISPID map ──
static const std::pair<const wchar_t*, DISPID> kMethodMap[] = {
    { L"GetModulesList",     DISPID_GET_MODULES_LIST },
    { L"GetRunningModuleId", DISPID_GET_RUNNING_MODULE_ID },
    { L"StartWallpaper",     DISPID_START_WALLPAPER },
    { L"StopWallpaper",      DISPID_STOP_WALLPAPER },
    { L"LoadSettingsUI",     DISPID_LOAD_SETTINGS_UI },
    { L"ApplySettings",      DISPID_APPLY_SETTINGS },
    { L"PickFile",           DISPID_PICK_FILE },
    { L"InstallModule",      DISPID_INSTALL_MODULE },
    { L"DetachWallpaper",    DISPID_DETACH_WALLPAPER },
    { L"AttachWallpaper",    DISPID_ATTACH_WALLPAPER },
    { L"GetIsAttached",      DISPID_GET_IS_ATTACHED },
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

NativeBridge::NativeBridge(WallpaperController* controller, QWidget* parentWindow)
    : m_controller(controller), m_parentWindow(parentWindow) {
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
                // Returns the module ID as a string — JS must parse with Number().
                result = std::to_string(m_controller->getRunningModuleId());
                break;

            case DISPID_START_WALLPAPER: {
                // Extract the module index from the first argument.
                // COM passes arguments in reverse order (right-to-left).
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
                // Extract the module index (second arg) and JSON string (first arg).
                // COM reverses argument order, so rgvarg[0] is the last arg.
                int idx = 0;
                std::string jsonStr;
                if (pDispParams->cArgs >= 2) {
                    VariantChangeType(&pDispParams->rgvarg[1],
                        &pDispParams->rgvarg[1], 0, VT_I4);
                    idx = pDispParams->rgvarg[1].intVal;

                    if (pDispParams->rgvarg[0].vt == VT_BSTR) {
                        std::wstring ws(pDispParams->rgvarg[0].bstrVal);
                        // Use QString for proper UTF-16 → UTF-8 conversion.
                        // Truncating wchar_t to char directly corrupts any
                        // character above U+00FF (e.g. CJK codepoints).
                        jsonStr = QString::fromStdWString(ws).toStdString();
                    }
                }
                QJsonDocument doc = QJsonDocument::fromJson(
                    QString::fromStdString(jsonStr).toUtf8());
                m_controller->applySettings(idx, doc.object());
                break;
            }

            case DISPID_PICK_FILE: {
                // Open a native Windows file dialog on the Qt main thread.
                // Returns the selected file path as a string, or empty string
                // if the user cancelled. A filter string can be passed as the
                // first optional argument (e.g. "Videos (*.mp4 *.avi)").
                QString filter = QStringLiteral("All Files (*.*)");
                if (pDispParams->cArgs >= 1) {
                    if (pDispParams->rgvarg[0].vt == VT_BSTR) {
                        std::wstring ws(pDispParams->rgvarg[0].bstrVal);
                        filter = QString::fromStdWString(ws);
                    }
                }
                QString path = QFileDialog::getOpenFileName(
                    m_parentWindow,
                    QStringLiteral("Select File"),
                    QString(),
                    filter);
                if (!path.isEmpty()) {
                    result = path.toStdString();
                }
                break;
            }

            case DISPID_INSTALL_MODULE: {
                // Open a native file dialog for *.zip on the Qt main thread.
                // NativeBridge has m_parentWindow for proper dialog parenting.
                qDebug() << "starting to install module";
                QString zipPath = QFileDialog::getOpenFileName(
                    m_parentWindow,
                    QStringLiteral("Select Module Package"),
                    QString(),
                    QStringLiteral("ZIP files (*.zip)"));
                qDebug() << "file selected" << zipPath;
                if (zipPath.isEmpty()) {
                    break; // User cancelled — return empty result.
                }
                qDebug() << "installing module now";
                // Delegate to the controller for validation + extraction.
                QString errMsg = m_controller->installModuleFromPath(zipPath);
                if (errMsg.isEmpty()) {
                    result = "OK";
                } else {
                    result = "ERROR:" + errMsg.toStdString();
                }
                break;
            }

            case DISPID_DETACH_WALLPAPER:
                m_controller->detachWallpaper();
                break;

            case DISPID_ATTACH_WALLPAPER:
                m_controller->attachWallpaper();
                break;

            case DISPID_GET_IS_ATTACHED:
                // Returns "1" if attached, "0" otherwise — JS must parse with Number().
                result = m_controller->getIsAttached() ? "1" : "0";
                break;

            default:
                hr = DISP_E_MEMBERNOTFOUND;
                break;
        }
    }, connType);

    // Return the result as a BSTR. If there's no result, return VT_EMPTY.
    if (pVarResult && !result.empty()) {
        V_VT(pVarResult) = VT_BSTR;
        // Use QString for proper UTF-8 → UTF-16 conversion. The naive
        // byte-for-byte widening (std::wstring(begin, end)) corrupts
        // multi-byte characters like CJK codepoints in file paths.
        std::wstring wresult = QString::fromStdString(result).toStdWString();
        pVarResult->bstrVal = SysAllocString(wresult.c_str());
    } else if (pVarResult) {
        V_VT(pVarResult) = VT_EMPTY;
    }

    return hr;
}

void NativeBridge::setMessageCallback(MessageCallback callback) {
    m_callback = std::move(callback);
}
