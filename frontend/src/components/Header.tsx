/**
 * @file components/Header.tsx
 * @brief Application header bar with title, Attach/Detach toggle, divider,
 *        and Stop Wallpaper button.
 *
 * Displays the "Animura" title on the left. When a wallpaper module is
 * running (`runningModuleId >= 0`), two action buttons appear on the right:
 *
 * - **Attach / Detach** — a glass-style button that toggles whether the
 *   running module window is attached to the desktop WorkerW or detached
 *   as a standalone borderless window.
 * - **Divider** — a thin, low-opacity vertical line separating the two
 *   buttons, floating with top/bottom padding.
 * - **Stop Wallpaper** — the existing stop button (unchanged).
 */

import React from 'react';

interface HeaderProps {
  /** The currently running module ID, or -1 if nothing is running. */
  runningModuleId: number;
  /** Whether the running module window is attached to the desktop. */
  isAttached: boolean;
  /** Whether auto-restore last wallpaper on startup is enabled. */
  restoreLast: boolean;
  /** Called when the user clicks "Stop Wallpaper". */
  onStop: () => void;
  /** Called when the user clicks "Detach" (detach from desktop). */
  onDetach: () => void;
  /** Called when the user clicks "Attach" (attach back to desktop). */
  onAttach: () => void;
  /** Called when the restore-last-wallpaper toggle is clicked. */
  onToggleRestore: () => void;
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
  /**
   * Glass-style attach/detach button.
   *
   * A minimal, frosted look that integrates with the Animura design:
   * translucent surface, subtle border, small backdrop blur, soft lift
   * on hover. Stays visually subordinate to the Stop button.
   */
  glassBtn: {
    padding: '8px 18px',
    fontSize: 13,
    fontWeight: 500,
    color: 'var(--text-secondary)',
    background: 'rgba(255, 255, 255, 0.03)',
    border: '1px solid rgba(255, 255, 255, 0.05)',
    borderRadius: 'var(--radius-button)',
    cursor: 'pointer',
    transition: 'all var(--transition-normal)',
    fontFamily: 'inherit',
    backdropFilter: 'blur(8px)',
    WebkitBackdropFilter: 'blur(8px)',
  },
  /**
   * Vertical divider between the two action buttons.
   *
   * Thin (1px), low-opacity line that floats centered in the header —
   * it uses a fixed height with top/bottom margin so it never touches
   * the header edges.
   */
  divider: {
    width: 1,
    height: 28,
    margin: '0 2px',
    background: 'rgba(255, 255, 255, 0.08)',
    flexShrink: 0,
  },
};

export const Header: React.FC<HeaderProps> = ({
  runningModuleId,
  isAttached,
  restoreLast,
  onStop,
  onDetach,
  onAttach,
  onToggleRestore,
}) => {
  const [stopHovered, setStopHovered] = React.useState(false);
  const [glassHovered, setGlassHovered] = React.useState(false);
  const [restoreHovered, setRestoreHovered] = React.useState(false);
  const [restorePressed, setRestorePressed] = React.useState(false);

  const isRunning = runningModuleId >= 0;

  return (
    <div style={styles.header}>
      <span style={styles.title}>Animura</span>

      {/* Restore-on-startup toggle */}
      <button
        onClick={onToggleRestore}
        onMouseEnter={() => setRestoreHovered(true)}
        onMouseLeave={() => { setRestoreHovered(false); setRestorePressed(false); }}
        onMouseDown={() => setRestorePressed(true)}
        onMouseUp={() => setRestorePressed(false)}
        title={restoreLast
          ? 'Auto-restore wallpaper on startup (enabled)'
          : 'Auto-restore wallpaper on startup (disabled)'}
        style={{
          marginLeft: 18,
          padding: '5px 14px',
          fontSize: 12,
          fontWeight: 500,
          color: restoreLast
            ? 'var(--accent)'
            : restoreHovered
              ? 'var(--text-secondary)'
              : '#564a6e',
          background: restoreLast
            ? restorePressed
              ? 'rgba(224,64,144,0.18)'
              : restoreHovered
                ? 'rgba(224,64,144,0.10)'
                : 'rgba(224,64,144,0.06)'
            : restorePressed
              ? 'rgba(255,255,255,0.04)'
              : restoreHovered
                ? 'rgba(255,255,255,0.03)'
                : 'rgba(255,255,255,0.015)',
          border: restoreLast
            ? restoreHovered
              ? '1px solid rgba(224,64,144,0.3)'
              : '1px solid rgba(224,64,144,0.15)'
            : restoreHovered
              ? '1px solid rgba(255,255,255,0.08)'
              : '1px solid rgba(255,255,255,0.04)',
          borderRadius: 'var(--radius-button)',
          cursor: 'pointer',
          fontFamily: 'inherit',
          transition: 'all 150ms',
          transform: restorePressed ? 'scale(0.97)' : 'scale(1)',
          whiteSpace: 'nowrap',
        }}
      >
        {restoreLast ? 'Restore on startup' : 'Restore on startup'}
      </button>

      <div style={styles.spacer} />

      {isRunning && (
        <>
          {/* Attach / Detach toggle — glass-style button */}
          <button
            style={{
              ...styles.glassBtn,
              background: glassHovered
                ? 'rgba(255, 255, 255, 0.06)'
                : 'rgba(255, 255, 255, 0.0)',
              borderColor: glassHovered
                ? 'rgba(255, 255, 255, 0.1)'
                : 'rgba(255, 255, 255, 0.0)',
              color: glassHovered
                ? 'var(--text-primary)'
                : 'var(--text-secondary)',
              transform: glassHovered ? 'translateY(-1px)' : 'translateY(0)',
              boxShadow: glassHovered
                ? '0 2px 8px rgba(0, 0, 0, 0.15)'
                : 'none',
              marginRight: 5
            }}
            onMouseEnter={() => setGlassHovered(true)}
            onMouseLeave={() => setGlassHovered(false)}
            onClick={isAttached ? onDetach : onAttach}
          >
            {isAttached ? 'Detach' : 'Attach'}
          </button>

          {/* Floating vertical divider */}
          <div style={styles.divider} />

          {/* Stop Wallpaper — existing style, unchanged */}
          <button
            style={{
              ...styles.stopBtn,
              background: stopHovered
                ? 'rgba(224,64,144,0.12)'
                : 'transparent',
              borderColor: stopHovered
                ? 'rgba(224,64,144,0.3)'
                : 'transparent',
              marginLeft: 5
            }}
            onMouseEnter={() => setStopHovered(true)}
            onMouseLeave={() => setStopHovered(false)}
            onClick={onStop}
          >
            Stop Wallpaper
          </button>
        </>
      )}
    </div>
  );
};
