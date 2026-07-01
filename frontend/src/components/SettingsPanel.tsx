import React from 'react';
import { SettingsControl } from './SettingsControl';

interface SettingsPanelProps {
  open: boolean;
  moduleId: number;
  schema: Record<string, unknown>;
  settings: Record<string, unknown>;
  hasChanged: boolean;
  onUpdateSetting: (key: string, value: unknown, path: string[]) => void;
  onApply: () => void;
  onToggleStartStop: () => void;
  onClose: () => void;
  isRunning: boolean;
}

export const SettingsPanel: React.FC<SettingsPanelProps> = ({
  open,
  moduleId,
  schema,
  settings,
  hasChanged,
  onUpdateSetting,
  onApply,
  onToggleStartStop,
  onClose,
  isRunning,
}) => {
  const [applyHovered, setApplyHovered] = React.useState(false);
  const [applyPressed, setApplyPressed] = React.useState(false);
  const [toggleHovered, setToggleHovered] = React.useState(false);
  const [togglePressed, setTogglePressed] = React.useState(false);

  if (moduleId < 0) return null;

  return (
    <>
      {/* Backdrop */}
      {open && (
        <div
          onClick={onClose}
          style={{
            position: 'fixed',
            inset: 0,
            zIndex: 20,
            background: 'rgba(0,0,0,0.2)',
            transition: 'opacity 200ms',
          }}
        />
      )}

      {/* Drawer */}
      <div
        style={{
          position: 'fixed',
          top: 0,
          right: 0,
          bottom: 0,
          width: 420,
          zIndex: 30,
          background: '#0f0825',
          transform: open ? 'translateX(0)' : 'translateX(100%)',
          transition: 'transform 280ms cubic-bezier(0.16, 1, 0.3, 1)',
          display: 'flex',
          flexDirection: 'column',
          boxShadow: open ? '-4px 0 24px rgba(0,0,0,0.5)' : 'none',
          // Left gradient edge
          borderLeft: '2px solid transparent',
          borderImage:
            'linear-gradient(180deg, transparent, rgba(224,64,144,0.3), transparent) 1',
        }}
      >
        {/* Header */}
        <div
          style={{
            height: 60,
            display: 'flex',
            alignItems: 'center',
            padding: '0 28px 0 28px',
            flexShrink: 0,
            position: 'relative',
          }}
        >
          <span
            style={{
              fontSize: 18,
              fontWeight: 600,
              color: 'var(--text-primary)',
              flex: 1,
            }}
          >
            Module Settings
          </span>
          <button
            onClick={onClose}
            style={{
              background: 'none',
              border: 'none',
              color: 'var(--text-secondary)',
              fontSize: 13,
              cursor: 'pointer',
              padding: '6px 12px',
              borderRadius: 10,
              fontFamily: 'inherit',
            }}
          >
            Close
          </button>
          {/* Bottom separator */}
          <div
            style={{
              position: 'absolute',
              bottom: 0,
              left: 16,
              right: 16,
              height: 1,
              background:
                'linear-gradient(90deg, transparent, rgba(224,64,144,0.25), transparent)',
            }}
          />
        </div>

        {/* Scrollable settings */}
        <div
          style={{
            flex: 1,
            overflow: 'auto',
            padding: 24,
          }}
        >
          <SettingsControl
            schemaObj={schema as Record<string, { type?: string }>}
            settingsObj={settings}
            path={[]}
            onUpdate={onUpdateSetting}
          />
        </div>

        {/* Footer */}
        <div
          style={{
            height: 140,
            padding: 24,
            background: '#0b0520',
            borderTop:
              '1px solid transparent',
            borderImage:
              'linear-gradient(90deg, transparent, rgba(224,64,144,0.2), transparent) 1',
            flexShrink: 0,
            display: 'flex',
            flexDirection: 'column',
            gap: 12,
          }}
        >
          {/* Apply Button */}
          <button
            disabled={!hasChanged}
            onClick={onApply}
            onMouseEnter={() => setApplyHovered(true)}
            onMouseLeave={() => { setApplyHovered(false); setApplyPressed(false); }}
            onMouseDown={() => setApplyPressed(true)}
            onMouseUp={() => setApplyPressed(false)}
            style={{
              width: '100%',
              height: 44,
              borderRadius: 'var(--radius-button)',
              border: hasChanged
                ? applyHovered
                  ? '1px solid rgba(255,100,200,0.5)'
                  : '1px solid rgba(255,64,180,0.3)'
                : '1px solid rgba(255,255,255,0.04)',
              background: hasChanged
                ? applyPressed
                  ? 'linear-gradient(180deg, #c03078 0%, #a02860 100%)'
                  : applyHovered
                    ? 'linear-gradient(180deg, #f050a0 0%, #d04088 100%)'
                    : 'linear-gradient(180deg, #e04090 0%, #c03078 100%)'
                : 'linear-gradient(180deg, #1a1035 0%, #150d2a 100%)',
              color: hasChanged ? 'var(--bg-base)' : '#564a6e',
              fontSize: 14,
              fontWeight: 500,
              cursor: hasChanged ? 'pointer' : 'default',
              fontFamily: 'inherit',
              transition: 'all 150ms',
              boxShadow: hasChanged && applyHovered
                ? '0 0 16px rgba(224,64,144,0.25)'
                : 'none',
              transform: applyPressed ? 'scale(0.97)' : 'scale(1)',
            }}
          >
            Apply
          </button>

          {/* Start/Stop Button */}
          <button
            onClick={onToggleStartStop}
            onMouseEnter={() => setToggleHovered(true)}
            onMouseLeave={() => { setToggleHovered(false); setTogglePressed(false); }}
            onMouseDown={() => setTogglePressed(true)}
            onMouseUp={() => setTogglePressed(false)}
            style={{
              width: '100%',
              height: 44,
              borderRadius: 'var(--radius-button)',
              border: isRunning
                ? toggleHovered
                  ? '1px solid rgba(224,64,144,0.55)'
                  : '1px solid rgba(224,64,144,0.35)'
                : toggleHovered
                  ? '1px solid rgba(255,255,255,0.18)'
                  : '1px solid rgba(255,255,255,0.08)',
              background: isRunning
                ? togglePressed
                  ? 'rgba(224,64,144,0.2)'
                  : toggleHovered
                    ? 'rgba(224,64,144,0.12)'
                    : 'transparent'
                : togglePressed
                  ? 'rgba(255,255,255,0.06)'
                  : toggleHovered
                    ? 'rgba(255,255,255,0.04)'
                    : 'transparent',
              color: isRunning ? 'var(--accent)' : 'var(--text-primary)',
              fontSize: 14,
              fontWeight: 500,
              cursor: 'pointer',
              fontFamily: 'inherit',
              transition: 'all 150ms',
              boxShadow: toggleHovered
                ? isRunning
                  ? '0 0 14px rgba(224,64,144,0.2)'
                  : '0 0 14px rgba(255,255,255,0.06)'
                : 'none',
              transform: togglePressed ? 'scale(0.97)' : 'scale(1)',
            }}
          >
            {isRunning ? 'Stop' : 'Start'}
          </button>
        </div>
      </div>
    </>
  );
};
