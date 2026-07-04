/**
 * @file App.tsx
 * @brief Root layout component for the Animura React frontend.
 *
 * Wires backend hooks to UI components. The layout is:
 * - **Header** — app title + conditionally-visible "Stop Wallpaper" button.
 * - **ModuleGrid** — responsive grid of available wallpaper module cards.
 * - **SettingsPanel** — slide-in drawer with auto-generated settings form
 *   (recursive SettingsControl), Apply button, and Start/Stop toggle.
 * - **Notification** — floating toast notification queue, centered at top.
 *
 * ## State Flow
 * 1. `useModules()` fetches the module list on mount.
 * 2. `useRunningModuleId()` tracks the active module via polling + WebView2 messages.
 * 3. User clicks a module card → `selectedModule` is set → SettingsPanel opens.
 * 4. `useSettings(selectedModule.id)` loads schema + current settings.
 * 5. User edits settings → `updateSetting` modifies local state → `hasChanged = true`.
 * 6. User clicks "Apply" → `applyChanges` writes to disk via NativeBridge.
 * 7. User clicks "Start"/"Stop" → `toggle` calls startWallpaper/stopWallpaper.
 */

import React from 'react';
import { Header } from './components/Header';
import { ModuleGrid } from './components/ModuleGrid';
import { SettingsPanel } from './components/SettingsPanel';
import { NotificationBanner } from './components/Notification';
import { useModules, useRunningModuleId, useSettings, useStartStop } from './hooks/useBackend';
import * as bridge from './bridge/native';
import { useNotifications } from './hooks/useNotifications';
import type { ModuleInfo } from './types';

const App: React.FC = () => {
  const { modules, loading } = useModules();
  const { runningModuleId, refresh } = useRunningModuleId();
  const { notifications, show, dismiss } = useNotifications();

  const [selectedModule, setSelectedModule] = React.useState<ModuleInfo | null>(null);
  const [panelOpen, setPanelOpen] = React.useState(false);

  const { schema, settings, hasChanged, updateSetting, applyChanges } = useSettings(
    selectedModule?.id ?? -1
  );
  const { isRunning, toggle } = useStartStop(
    selectedModule?.id ?? -1,
    runningModuleId,
    refresh,
  );

  /** Opens the settings panel for a clicked module. */
  const handleModuleClick = React.useCallback(
    (mod: ModuleInfo) => {
      setSelectedModule(mod);
      setPanelOpen(true);
    },
    []
  );

  /** Applies settings and shows a success toast. */
  const handleApply = React.useCallback(async () => {
    await applyChanges();
    show('Settings applied', 'success');
  }, [applyChanges, show]);

  /** Toggles the wallpaper module start/stop. */
  const handleToggleStartStop = React.useCallback(async () => {
    await toggle();
  }, [toggle]);

  /** Stops the wallpaper from the header button. */
  const handleStop = React.useCallback(async () => {
    await bridge.stopWallpaper();
    await refresh();
  }, [refresh]);

  const handleClosePanel = React.useCallback(() => {
    setPanelOpen(false);
  }, []);

  return (
    <div
      style={{
        width: '100vw',
        height: '100vh',
        display: 'flex',
        flexDirection: 'column',
        background: 'var(--bg-base)',
        overflow: 'hidden',
        position: 'relative',
      }}
    >
      <Header runningModuleId={runningModuleId} onStop={handleStop} />

      <div style={{ flex: 1, overflow: 'hidden', position: 'relative' }}>
        <ModuleGrid
          modules={modules}
          loading={loading}
          onModuleClick={handleModuleClick}
        />

        <SettingsPanel
          open={panelOpen}
          moduleId={selectedModule?.id ?? -1}
          schema={schema}
          settings={settings}
          hasChanged={hasChanged}
          onUpdateSetting={updateSetting}
          onApply={handleApply}
          onToggleStartStop={handleToggleStartStop}
          onClose={handleClosePanel}
          isRunning={isRunning}
        />
      </div>

      {/* Notifications — centered at top, above all other content */}
      <div
        style={{
          position: 'fixed',
          top: 8,
          left: '50%',
          transform: 'translateX(-50%)',
          zIndex: 100,
          display: 'flex',
          flexDirection: 'column',
          gap: 6,
          width: 'calc(100% - 16px)',
          maxWidth: 500,
          pointerEvents: 'none',
        }}
      >
        {notifications.map((n) => (
          <NotificationBanner
            key={n.id}
            notification={n}
            onDone={() => dismiss(n.id)}
          />
        ))}
      </div>
    </div>
  );
};

export default App;
