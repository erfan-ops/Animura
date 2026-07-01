import { useState, useCallback, useRef } from 'react';
import type { NotificationType } from '../types';

export interface Notification {
  id: number;
  message: string;
  type: NotificationType;
}

export function useNotifications() {
  const [notifications, setNotifications] = useState<Notification[]>([]);
  const nextId = useRef(0);

  const show = useCallback((message: string, type: NotificationType = 'success') => {
    const id = nextId.current++;
    const notification: Notification = { id, message, type };

    setNotifications((prev) => {
      if (prev.length >= 5) return prev;
      return [...prev, notification];
    });

    // Auto-dismiss after 3s
    setTimeout(() => {
      setNotifications((prev) => prev.filter((n) => n.id !== id));
    }, 3000);
  }, []);

  const dismiss = useCallback((id: number) => {
    setNotifications((prev) => prev.filter((n) => n.id !== id));
  }, []);

  return { notifications, show, dismiss };
}
