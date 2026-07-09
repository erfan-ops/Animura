/**
 * @file components/ModuleGrid.tsx
 * @brief Responsive CSS grid that renders available wallpaper modules as
 *        ModuleCard components.
 *
 * When `loading` is true, displays a "Loading modules..." placeholder.
 * Otherwise, renders a centered auto-fill grid with 280px column tracks
 * and 20px gaps. Each ModuleCard is wrapped in a center-justified flex
 * container so cards stay centered regardless of grid column count.
 */

import React from 'react';
import type { ModuleInfo } from '../types';
import { ModuleCard } from './ModuleCard';

interface ModuleGridProps {
  /** Array of modules to display (from `useModules()`). */
  modules: ModuleInfo[];
  /** Whether the initial module list fetch is still in progress. */
  loading: boolean;
  /** Called when a module card is clicked. Passes the selected ModuleInfo. */
  onModuleClick: (module: ModuleInfo) => void;
  /** Called when the "Add Module" button is clicked. */
  onInstallModule: () => void;
}

export const ModuleGrid: React.FC<ModuleGridProps> = ({
  modules,
  loading,
  onModuleClick,
  onInstallModule,
}) => {
  const [addHovered, setAddHovered] = React.useState(false);

  if (loading) {
    return (
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          height: '100%',
        }}
      >
        <span style={{ color: 'var(--text-secondary)', fontSize: 14 }}>
          Loading modules...
        </span>
      </div>
    );
  }

  return (
    <div
      style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(auto-fill, 280px)',
        gap: 20,
        padding: 36,
        paddingTop: 16,
        justifyContent: 'center',
        alignContent: 'start',
        overflow: 'auto',
        height: '100%',
      }}
    >
      {/* "Add Module" card — opens the ZIP package installer */}
      <div key="add-module" style={{ display: 'flex', justifyContent: 'center' }}>
        <div
          onClick={onInstallModule}
          onMouseEnter={() => setAddHovered(true)}
          onMouseLeave={() => setAddHovered(false)}
          style={{
            width: 260,
            height: 220,
            position: 'relative',
            cursor: 'pointer',
            transform: addHovered ? 'scale(1.04)' : 'scale(1)',
            transition: 'transform 200ms cubic-bezier(0.16, 1, 0.3, 1)',
          }}
        >
          {/* Shadow layer */}
          <div
            style={{
              position: 'absolute',
              inset: 0,
              borderRadius: 'var(--radius-card)',
              background: 'var(--shadow-dark)',
              opacity: addHovered ? 0.7 : 0.5,
              transform: `translate(${addHovered ? 5 : 3}px, ${addHovered ? 6 : 4}px)`,
              transition: 'opacity 200ms, transform 200ms',
            }}
          />

          {/* Card surface — dashed border to indicate "add" action */}
          <div
            style={{
              position: 'absolute',
              inset: 0,
              borderRadius: 'var(--radius-card)',
              background: addHovered
                ? 'linear-gradient(180deg, #281d48 0%, #1e1438 100%)'
                : 'linear-gradient(180deg, #1e1438 0%, #160d2a 100%)',
              border: `1px dashed ${addHovered ? 'rgba(224,64,144,0.5)' : 'rgba(255,255,255,0.1)'}`,
              transition: 'all 180ms',
              overflow: 'hidden',
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              boxShadow: `0 0 ${addHovered ? '10' : '0'}px rgba(224,64,144,0.7)`,
            }}
          >
            {/* Plus icon */}
            <div
              style={{
                fontSize: 48,
                color: addHovered ? 'var(--accent)' : 'rgba(255,255,255,0.25)',
                fontWeight: 200,
                lineHeight: 1,
                marginBottom: 12,
                transition: 'color 180ms',
                userSelect: 'none',
              }}
            >
              +
            </div>

            {/* Label */}
            <span
              style={{
                fontSize: 14,
                fontWeight: 500,
                color: addHovered ? 'var(--text-primary)' : 'var(--text-secondary)',
                transition: 'color 180ms',
                userSelect: 'none',
              }}
            >
              Add Module
            </span>
          </div>
        </div>
      </div>

      {modules.map((mod) => (
        <div key={mod.id} style={{ display: 'flex', justifyContent: 'center' }}>
          <ModuleCard module={mod} onClick={() => onModuleClick(mod)} />
        </div>
      ))}
    </div>
  );
};
