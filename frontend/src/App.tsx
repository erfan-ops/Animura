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

  const handleModuleClick = React.useCallback(
    (mod: ModuleInfo) => {
      setSelectedModule(mod);
      setPanelOpen(true);
    },
    []
  );

  const handleApply = React.useCallback(async () => {
    await applyChanges();
    show('Settings applied', 'success');
  }, [applyChanges, show]);

  const handleToggleStartStop = React.useCallback(async () => {
    await toggle();
  }, [toggle]);

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

      {/* Notifications */}
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
