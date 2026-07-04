/**
 * @file hooks/useNotifications.ts
 * @brief Lightweight toast notification system for the React UI.
 *
 * Provides a queue-based notification system with auto-dismiss. Used to
 * give feedback after settings are applied or wallpaper is started/stopped.
 *
 * ## Limits
 * - Maximum 5 concurrent notifications to prevent stacking overflow.
 * - Auto-dismiss after 3 seconds.
 * - Manual dismiss available via `dismiss(id)`.
 */

import { useState, useCallback, useRef } from 'react';
import type { NotificationType } from '../types';

/** A single toast notification entry. */
export interface Notification {
  /** Unique auto-incrementing ID. */
  id: number;
  /** The message text displayed in the toast. */
  message: string;
  /** Severity level — drives accent color in the banner. */
  type: NotificationType;
}

/**
 * Hook that manages a toast notification queue.
 *
 * @returns `{ notifications, show, dismiss }`
 *   - `notifications` — array of active notifications.
 *   - `show(message, type)` — enqueue a new notification.
 *   - `dismiss(id)` — immediately remove a notification by ID.
 */
export function useNotifications() {
  const [notifications, setNotifications] = useState<Notification[]>([]);
  const nextId = useRef(0);

  /**
   * Shows a new notification.
   *
   * If the queue is at capacity (5 notifications), the new notification
   * is silently dropped to prevent visual overflow.
   *
   * Each notification auto-dismisses after 3 seconds.
   *
   * @param message The text to display.
   * @param type    Severity level (default: 'success').
   */
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

  /** Immediately removes a notification by its ID. */
  const dismiss = useCallback((id: number) => {
    setNotifications((prev) => prev.filter((n) => n.id !== id));
  }, []);

  return { notifications, show, dismiss };
}
