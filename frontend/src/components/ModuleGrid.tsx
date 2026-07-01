import React from 'react';
import type { ModuleInfo } from '../types';
import { ModuleCard } from './ModuleCard';

interface ModuleGridProps {
  modules: ModuleInfo[];
  loading: boolean;
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
