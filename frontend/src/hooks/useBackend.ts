/**
 * @file hooks/useBackend.ts
 * @brief React hooks for communicating with the C++ backend via NativeBridge.
 *
 * These hooks encapsulate all C++ ↔ JS communication for the React UI:
 * - `useModules` — fetches the module list once on mount.
 * - `useRunningModuleId` — tracks the active module via polling and
 *   WebView2 web messages.
 * - `useSettings` — loads schema/settings when the selected module changes
 *   and provides `updateSetting` / `applyChanges` for the settings form.
 * - `useStartStop` — toggles the module's running state and forces a sync
 *   after the action completes.
 */

import { useState, useEffect, useCallback } from 'react';
import type { ModuleInfo, SettingsUI } from '../types';
import * as bridge from '../bridge/native';

/**
 * Fetches the list of available wallpaper modules on mount, and refreshes
 * when the C++ backend emits a `modulesChanged` web message.
 *
 * @returns `{ modules, loading, refresh }` — the module array, whether the
 *          initial fetch is in progress, and a function to force a refresh.
 */
export function useModules() {
  const [modules, setModules] = useState<ModuleInfo[]>([]);
  const [loading, setLoading] = useState(true);

  const refresh = useCallback(async () => {
    const list = await bridge.getModulesList();
    setModules(list);
  }, []);

  useEffect(() => {
    bridge.getModulesList().then((list) => {
      setModules(list);
      setLoading(false);
    });

    // Listen for catalog changes pushed from C++ (e.g., after runtime install).
    const unsub = bridge.onBackendMessage((msg) => {
      if (msg.type === 'modulesChanged') {
        bridge.getModulesList().then(setModules);
      }
    });

    return unsub;
  }, []);

  return { modules, loading, refresh };
}

/**
 * Tracks the ID of the currently running wallpaper module.
 *
 * Uses two mechanisms:
 * 1. **Initial poll** — calls `getRunningModuleId()` on mount.
 * 2. **WebView2 messages** — listens for `runningModuleChanged` messages
 *    pushed from C++ when a module starts or stops.
 * 3. **Manual refresh** — `refresh()` forces an immediate poll. Called
 *    after start/stop actions because the WebView2 message is asynchronous
 *    and may not arrive before the next React render cycle.
 *
 * @returns `{ runningModuleId, refresh }` — the current running module ID
 *          (-1 if none) and a function to force a poll.
 */
export function useRunningModuleId() {
  const [runningModuleId, setRunningModuleId] = useState(-1);

  useEffect(() => {
    bridge.getRunningModuleId().then(setRunningModuleId);

    const unsub = bridge.onBackendMessage((msg) => {
      if (msg.type === 'runningModuleChanged') {
        setRunningModuleId(msg.moduleId);
      }
    });

    return unsub;
  }, []);

  // Force an immediate poll of the C++ backend after a start/stop action,
  // rather than waiting for the async WebView2 message to arrive.
  const refresh = useCallback(async () => {
    const id = await bridge.getRunningModuleId();
    setRunningModuleId(id);
  }, []);

  return { runningModuleId, refresh };
}

/**
 * Manages the settings schema and values for a selected module.
 *
 * When `moduleId` changes (user clicks a different module), reloads the
 * schema and current settings from disk via the C++ backend.
 *
 * `updateSetting` modifies a nested setting value using a path array
 * (e.g., `["stars", "color"]`). It creates shallow copies at each level
 * to trigger React re-renders.
 *
 * `applyChanges` writes the current settings to disk via the C++ backend
 * and resets the `hasChanged` flag.
 *
 * @param moduleId The index of the module whose settings are being edited.
 *                 Values < 0 mean no module is selected.
 * @returns `{ schema, settings, hasChanged, updateSetting, applyChanges }`
 */
export function useSettings(moduleId: number) {
  const [schema, setSchema] = useState<Record<string, unknown>>({});
  const [settings, setSettings] = useState<Record<string, unknown>>({});
  const [hasChanged, setHasChanged] = useState(false);

  useEffect(() => {
    if (moduleId < 0) return;
    bridge.loadSettingsUI(moduleId).then((data: SettingsUI) => {
      setSchema(data.schema);
      setSettings(data.settings);
      setHasChanged(false);
    });
  }, [moduleId]);

  /**
   * Updates a single setting value, navigating through nested path segments.
   *
   * Uses shallow copies at each level so React detects the state change.
   * Sets `hasChanged = true` to enable the Apply button.
   *
   * @param key   The setting key to update.
   * @param value The new value.
   * @param path  Array of parent group keys for nested settings (e.g., `["stars"]`).
   *              Empty for top-level settings.
   */
  const updateSetting = useCallback(
    (key: string, value: unknown, path: string[] = []) => {
      setSettings((prev) => {
        const next = { ...prev };
        let current: Record<string, unknown> = next;
        for (const p of path) {
          if (current[p] && typeof current[p] === 'object') {
            const nested = { ...(current[p] as Record<string, unknown>) };
            current[p] = nested;
            current = nested;
          } else {
            return next;
          }
        }
        current[key] = value;
        return next;
      });
      setHasChanged(true);
    },
    []
  );

  /**
   * Writes the current settings to disk via the C++ backend.
   * Uses QSaveFile on the C++ side for atomic writes.
   */
  const applyChanges = useCallback(async () => {
    await bridge.applySettings(moduleId, settings);
    setHasChanged(false);
    return true;
  }, [moduleId, settings]);

  return { schema, settings, hasChanged, updateSetting, applyChanges };
}

/**
 * Manages the Start/Stop toggle for a wallpaper module.
 *
 * `isRunning` is derived: `runningModuleId === moduleId`.
 * `toggle()` calls start or stop depending on current state, then
 * calls `refresh()` to force an immediate poll of the running module ID
 * rather than waiting for the async WebView2 message.
 *
 * @param moduleId        The module this toggle controls.
 * @param runningModuleId The currently running module's ID (from useRunningModuleId).
 * @param refresh         The refresh function from useRunningModuleId.
 * @returns `{ isRunning, toggle }`
 */
export function useStartStop(
  moduleId: number,
  runningModuleId: number,
  refresh: () => Promise<void>,
) {
  const isRunning = runningModuleId === moduleId;

  const toggle = useCallback(async () => {
    if (isRunning) {
      await bridge.stopWallpaper();
    } else {
      await bridge.startWallpaper(moduleId);
    }
    // Force an immediate state sync after the action completes.
    // The C++ signal sends a WebView2 message asynchronously, but we
    // can't rely on it arriving before the next React render cycle.
    await refresh();
  }, [isRunning, moduleId, refresh]);

  return { isRunning, toggle };
}
