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
}

export const ModuleGrid: React.FC<ModuleGridProps> = ({
  modules,
  loading,
  onModuleClick,
}) => {
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
      {modules.map((mod) => (
        <div key={mod.id} style={{ display: 'flex', justifyContent: 'center' }}>
          <ModuleCard module={mod} onClick={() => onModuleClick(mod)} />
        </div>
      ))}
    </div>
  );
};
