import type { NativeBridge, ModuleInfo, SettingsUI } from '../types';

/**
 * Typed wrapper around the WebView2 native bridge.
 * All C++ calls are async (WebView2 host object proxy returns Promises).
 */

function getBridge(): NativeBridge | null {
  if (window.chrome?.webview?.hostObjects?.nativeBridge) {
    return window.chrome.webview.hostObjects.nativeBridge;
  }
  return null;
}

export async function getModulesList(): Promise<ModuleInfo[]> {
  const bridge = getBridge();
  if (!bridge) return [];
  const json = await bridge.GetModulesList();
  return JSON.parse(json);
}

export async function getRunningModuleId(): Promise<number> {
  const bridge = getBridge();
  if (!bridge) return -1;
  // The COM bridge always returns BSTR (string), so parse it.
  const raw = await bridge.GetRunningModuleId();
  return Number(raw);
}

export async function startWallpaper(moduleIndex: number): Promise<void> {
  const bridge = getBridge();
  if (!bridge) return;
  await bridge.StartWallpaper(moduleIndex);
}

export async function stopWallpaper(): Promise<void> {
  const bridge = getBridge();
  if (!bridge) return;
  await bridge.StopWallpaper();
}

export async function loadSettingsUI(moduleIndex: number): Promise<SettingsUI> {
  const bridge = getBridge();
  if (!bridge) return { schema: {}, settings: {} };
  const json = await bridge.LoadSettingsUI(moduleIndex);
  return JSON.parse(json);
}

export async function applySettings(
  moduleIndex: number,
  settings: Record<string, unknown>
): Promise<void> {
  const bridge = getBridge();
  if (!bridge) return;
  await bridge.ApplySettings(moduleIndex, JSON.stringify(settings));
}

/**
 * Subscribe to C++ → JS messages.
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
