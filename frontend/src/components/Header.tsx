/**
 * @file components/Header.tsx
 * @brief Application header bar with title and conditional Stop Wallpaper button.
 *
 * Displays the "Animura" title on the left and a "Stop Wallpaper" button
 * on the right that appears only when a wallpaper module is running
 * (`runningModuleId >= 0`). The button has a subtle pink hover effect.
 */

import React from 'react';

interface HeaderProps {
  /** The currently running module ID, or -1 if nothing is running. */
  runningModuleId: number;
  /** Called when the user clicks "Stop Wallpaper". */
  onStop: () => void;
}

const styles: Record<string, React.CSSProperties> = {
  header: {
    height: 56,
    display: 'flex',
    alignItems: 'center',
    padding: '0 28px 0 28px',
    borderBottom: '1px solid transparent',
    borderImage:
      'linear-gradient(90deg, transparent, rgba(224,64,144,0.25), rgba(224,64,144,0.25), transparent) 1',
    flexShrink: 0,
  },
  title: {
    fontSize: 22,
    fontWeight: 600,
    color: 'var(--text-primary)',
    opacity: 0.95,
  },
  spacer: {
    flex: 1,
  },
  stopBtn: {
    padding: '8px 20px',
    fontSize: 13,
    fontWeight: 500,
    color: 'var(--accent)',
    background: 'transparent',
    border: '1px solid transparent',
    borderRadius: 'var(--radius-button)',
    cursor: 'pointer',
    transition: 'all var(--transition-normal)',
    fontFamily: 'inherit',
  },
};

export const Header: React.FC<HeaderProps> = ({ runningModuleId, onStop }) => {
  const [hovered, setHovered] = React.useState(false);

  return (
    <div style={styles.header}>
      <span style={styles.title}>Animura</span>
      <div style={styles.spacer} />
      {runningModuleId >= 0 && (
        <button
          style={{
            ...styles.stopBtn,
            background: hovered ? 'rgba(224,64,144,0.12)' : 'transparent',
            borderColor: hovered ? 'rgba(224,64,144,0.3)' : 'transparent',
          }}
          onMouseEnter={() => setHovered(true)}
          onMouseLeave={() => setHovered(false)}
          onClick={onStop}
        >
          Stop Wallpaper
        </button>
      )}
    </div>
  );
};
