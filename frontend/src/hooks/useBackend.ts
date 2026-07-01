import { useState, useEffect, useCallback } from 'react';
import type { ModuleInfo, SettingsUI } from '../types';
import * as bridge from '../bridge/native';

export function useModules() {
  const [modules, setModules] = useState<ModuleInfo[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    bridge.getModulesList().then((list) => {
      setModules(list);
      setLoading(false);
    });
  }, []);

  return { modules, loading };
}

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

  const applyChanges = useCallback(async () => {
    await bridge.applySettings(moduleId, settings);
    setHasChanged(false);
    return true;
  }, [moduleId, settings]);

  return { schema, settings, hasChanged, updateSetting, applyChanges };
}

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
