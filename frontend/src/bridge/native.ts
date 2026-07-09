/**
 * @file bridge/native.ts
 * @brief Typed wrapper around the WebView2 COM NativeBridge host object.
 *
 * The C++ NativeBridge is registered as `window.chrome.webview.hostObjects.nativeBridge`.
 * All C++ calls are asynchronous (WebView2 host object proxy returns Promises).
 *
 * ## BSTR Type Coercion
 * All COM bridge methods return BSTR (string) at runtime regardless of
 * TypeScript types. `getRunningModuleId()` returns a string like `"0"` and
 * MUST be parsed with `Number()`. The TypeScript `Promise<number>` return
 * type documents intent but the runtime coercion happens here.
 *
 * ## Out-of-WebView2 Fallback
 * When running in a regular browser (e.g., Vite dev server in a browser tab
 * instead of WebView2), `window.chrome` is undefined. All functions return
 * safe defaults (empty array, -1, etc.) so the UI renders gracefully.
 */

import type { NativeBridge, ModuleInfo, SettingsUI } from '../types';

/**
 * Returns the COM NativeBridge proxy, or null if not running inside WebView2.
 *
 * In WebView2, the bridge is registered via AddHostObjectToScript in
 * WebView2Host::initWebView2() and exposed as a synchronous proxy object
 * whose method calls return Promises.
 */
function getBridge(): NativeBridge | null {
  if (window.chrome?.webview?.hostObjects?.nativeBridge) {
    return window.chrome.webview.hostObjects.nativeBridge;
  }
  return null;
}

/**
 * Fetches the list of available wallpaper modules from the C++ backend.
 *
 * Calls `NativeBridge.GetModulesList()` which returns a JSON string.
 * The string is parsed into an array of ModuleInfo objects.
 *
 * @returns Array of module metadata, or empty array if the bridge is unavailable.
 */
export async function getModulesList(): Promise<ModuleInfo[]> {
  const bridge = getBridge();
  if (!bridge) return [];
  const json = await bridge.GetModulesList();
  return JSON.parse(json);
}

/**
 * Retrieves the ID of the currently running module.
 *
 * The COM bridge returns a BSTR (string) — we parse it with Number().
 * Returns -1 if no module is running or if the bridge is unavailable.
 *
 * @returns Module index (0-based) if running, -1 otherwise.
 */
export async function getRunningModuleId(): Promise<number> {
  const bridge = getBridge();
  if (!bridge) return -1;
  // The COM bridge always returns BSTR (string), so parse it.
  const raw = await bridge.GetRunningModuleId();
  return Number(raw);
}

/**
 * Starts a wallpaper module by its catalog index.
 *
 * This is a blocking call from the JS perspective — the C++ side uses
 * BlockingQueuedConnection to marshal the call to the Qt main thread and
 * waits for completion. The module starts on a worker thread, so the
 * actual render loop does not block the UI.
 *
 * @param moduleIndex Zero-based index of the module to start.
 */
export async function startWallpaper(moduleIndex: number): Promise<void> {
  const bridge = getBridge();
  if (!bridge) return;
  await bridge.StartWallpaper(moduleIndex);
}

/**
 * Stops the currently running wallpaper module and restores the original
 * Windows wallpaper.
 *
 * The C++ side detaches from the desktop, signals the module to stop via
 * atomic flag, joins the worker thread, and destroys the module on the
 * worker thread. This is also a blocking call from JS perspective.
 */
export async function stopWallpaper(): Promise<void> {
  const bridge = getBridge();
  if (!bridge) return;
  await bridge.StopWallpaper();
}

/**
 * Loads the settings schema and current values for a module's settings panel.
 *
 * Returns a `{ schema, settings }` object. The schema is a JSON Schema
 * defining types, ranges, and options. `SettingsControl` uses this to
 * recursively generate form controls.
 *
 * @param moduleIndex Index of the module to load settings for.
 * @returns Schema and current settings, or `{ schema: {}, settings: {} }` if unavailable.
 */
export async function loadSettingsUI(moduleIndex: number): Promise<SettingsUI> {
  const bridge = getBridge();
  if (!bridge) return { schema: {}, settings: {} };
  const json = await bridge.LoadSettingsUI(moduleIndex);
  return JSON.parse(json);
}

/**
 * Writes updated settings to the module's `settings.json` file.
 *
 * The C++ side uses QSaveFile for atomic writes (temp file + rename),
 * preventing corruption on crash.
 *
 * @param moduleIndex Index of the module to update.
 * @param settings    The complete settings object to write.
 */
export async function applySettings(
  moduleIndex: number,
  settings: Record<string, unknown>
): Promise<void> {
  const bridge = getBridge();
  if (!bridge) return;
  await bridge.ApplySettings(moduleIndex, JSON.stringify(settings));
}

/**
 * Opens a native file dialog (Windows file picker) and returns the full path
 * of the selected file.
 *
 * The C++ side uses `QFileDialog::getOpenFileName` on the Qt main thread.
 * An optional filter string can be passed to restrict visible file types
 * (e.g., `"Videos (*.mp4 *.avi)"`).
 *
 * @param filter Optional file filter string for the native dialog.
 * @returns The full path of the selected file, or an empty string if cancelled.
 */
export async function pickFile(filter?: string): Promise<string> {
  const bridge = getBridge();
  if (!bridge) return '';
  if (filter) {
    return await bridge.PickFile(filter);
  }
  return await bridge.PickFile();
}

/**
 * Opens a native file dialog for *.zip modules, installs the selected
 * package at runtime, and refreshes the module catalog — no restart needed.
 *
 * The C++ side handles the full workflow: file dialog, ZIP validation,
 * extraction, catalog update, and frontend notification.
 *
 * @returns "OK" on success, or an error string prefixed with "ERROR:" on failure.
 *          Returns an empty string if the user cancelled the file dialog.
 */
export async function installModule(): Promise<string> {
  const bridge = getBridge();
  if (!bridge) return 'ERROR: Native bridge not available.';
  return await bridge.InstallModule();
}

/**
 * Subscribes to C++ → JS web messages.
 *
 * The C++ side calls `PostWebMessageAsJson` with messages like:
 * ```json
 * {"type": "runningModuleChanged", "moduleId": 0}
 * ```
 *
 * Returns an unsubscribe function that removes the event listener.
 * Used by `useRunningModuleId` to react to module start/stop events.
 *
 * @param handler Callback invoked with each parsed message.
 * @returns A cleanup function that unsubscribes the handler.
 */
export function onBackendMessage(
  handler: (msg: { type: string; moduleId: number }) => void
): () => void {
  const listener = (e: MessageEvent<string>) => {
    try {
      const msg = JSON.parse(e.data);
      handler(msg);
    } catch {
      // Ignore malformed messages
    }
  };

  window.chrome?.webview?.addEventListener('message', listener);

  return () => {
    window.chrome?.webview?.removeEventListener('message', listener);
  };
}
