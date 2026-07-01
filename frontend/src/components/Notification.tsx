import React, { useEffect, useState } from 'react';
import type { Notification as Notif } from '../hooks/useNotifications';

interface NotificationBannerProps {
  notification: Notif;
  onDone: () => void;
}

const typeColors: Record<string, string> = {
  success: '#e04090',
  warning: '#ed6c02',
  error: '#e04040',
};

export const NotificationBanner: React.FC<NotificationBannerProps> = ({
  notification,
  onDone,
}) => {
  const [phase, setPhase] = useState<'enter' | 'show' | 'exit'>('enter');

  useEffect(() => {
    const enterTimer = setTimeout(() => setPhase('show'), 240);
    const exitTimer = setTimeout(() => setPhase('exit'), 2740);
    const doneTimer = setTimeout(() => onDone(), 2940);

    return () => {
      clearTimeout(enterTimer);
      clearTimeout(exitTimer);
      clearTimeout(doneTimer);
    };
  }, [onDone]);

  const accentColor = typeColors[notification.type] || '#e04090';

  const style: React.CSSProperties = {
    width: '100%',
    height: 44,
    borderRadius: 14,
    display: 'flex',
    alignItems: 'center',
    padding: '0 14px',
    gap: 10,
    background:
      notification.type === 'success'
        ? 'linear-gradient(90deg, #2a1040, #1a1035)'
        : notification.type === 'warning'
          ? 'linear-gradient(90deg, #3a1a10, #1a1035)'
          : 'linear-gradient(90deg, #3a1020, #1a1035)',
    transform:
      phase === 'enter'
        ? 'translateY(-100%)'
        : phase === 'exit'
          ? 'translateY(-100%)'
          : 'translateY(0)',
    opacity: phase === 'show' ? 1 : 0,
    transition: 'transform 240ms cubic-bezier(0.16, 1, 0.3, 1), opacity 160ms',
    pointerEvents: 'none',
  };

  return (
    <div style={style}>
      {/* Accent dot */}
      <div
        style={{
          width: 8,
          height: 8,
          borderRadius: '50%',
          backgroundColor: accentColor,
          flexShrink: 0,
        }}
      />
      <span
        style={{
          fontSize: 13,
          fontWeight: 500,
          color: 'var(--text-primary)',
        }}
      >
        {notification.message}
      </span>
    </div>
  );
};
