/**
 * @file components/Tooltip.tsx
 * @brief Glassmorphism tooltip rendered via React portal.
 *
 * A minimal, elegant tooltip with a frosted-glass appearance:
 * - Semi-transparent purple-tinted background with backdrop blur.
 * - Inner shadow highlights that simulate light catching the glass edges.
 * - Soft, contained shadows — no large external drop shadows.
 * - Smooth fade + scale animation on enter/exit.
 *
 * Positioning is viewport-aware: the tooltip prefers to appear above
 * the target element, flipping below if there isn't enough space.
 * Horizontal position is clamped to keep the tooltip on-screen.
 *
 * Rendered through `createPortal` to `document.body` so it is never
 * clipped by parent overflow or stacking contexts.
 */

import React, {
  useEffect,
  useLayoutEffect,
  useState,
  useRef,
} from 'react';
import { createPortal } from 'react-dom';

interface TooltipProps {
  /** Ref to the element the tooltip anchors to (used for positioning). */
  targetRef: React.RefObject<HTMLElement | null>;
  /** Whether the tooltip should be shown (drives enter/exit animations). */
  visible: boolean;
  /** Tooltip body content. */
  children: React.ReactNode;
}

export const Tooltip: React.FC<TooltipProps> = ({
  targetRef,
  visible,
  children,
}) => {
  const tooltipRef = useRef<HTMLDivElement>(null);

  /** Whether the tooltip DOM node exists (portal is mounted). */
  const [mounted, setMounted] = useState(false);

  /**
   * Controls the animated state.
   * - `false` → opacity 0, translateY(-4px), scale(0.96), no transition.
   * - `true`  → opacity 1, translateY(0), scale(1), with CSS transition.
   *
   * Enter: mount at `false` → paint → set `true` (transition plays in).
   * Exit:  set `false` → transition plays out → unmount after delay.
   */
  const [animating, setAnimating] = useState(false);

  /** Calculated fixed-position coordinates for the tooltip. */
  const [pos, setPos] = useState<{ top: number; left: number } | null>(null);

  /* ── Mount / unmount with exit-animation delay ── */
  useEffect(() => {
    if (visible) {
      setMounted(true);
      setAnimating(false);
    } else {
      setAnimating(false);
      const timer = setTimeout(() => setMounted(false), 250);
      return () => clearTimeout(timer);
    }
  }, [visible]);

  /* ── Position calculation (runs synchronously after render, before paint) ── */
  useLayoutEffect(() => {
    if (!mounted || !targetRef.current || !tooltipRef.current) return;

    const cardRect = targetRef.current.getBoundingClientRect();
    const tipRect = tooltipRef.current.getBoundingClientRect();

    const gap = 14;

    /* Prefer above the card; flip below if not enough viewport space. */
    let top = cardRect.top - tipRect.height - gap;
    if (top < 8) {
      top = cardRect.bottom + gap;
    }

    /* Center horizontally, clamped to viewport edges. */
    let left =
      cardRect.left + cardRect.width / 2 - tipRect.width / 2;
    left = Math.max(8, Math.min(left, window.innerWidth - tipRect.width - 8));

    setPos({ top, left });

    /* Trigger enter animation on the next paint cycle (double-rAF)
     * so the browser has an opacity-0 frame to transition from. */
    if (visible) {
      requestAnimationFrame(() => {
        requestAnimationFrame(() => {
          setAnimating(true);
        });
      });
    }
  }, [mounted, visible, targetRef]);

  /* ── Don't mount the portal until needed ── */
  if (!mounted) return null;

  const tooltipStyle: React.CSSProperties = {
    position: 'fixed',
    top: pos?.top ?? -9999,
    left: pos?.left ?? -9999,
    zIndex: 1000,
    pointerEvents: 'none',

    /* Glassmorphism surface */
    maxWidth: 320,
    minWidth: 180,
    padding: '14px 18px',
    borderRadius: 'var(--radius-card)',
    background:
      'linear-gradient(135deg, rgba(26, 16, 53, 0.55), rgba(13, 6, 32, 0.72))',
    backdropFilter: 'blur(24px)',
    WebkitBackdropFilter: 'blur(24px)',
    border: '1px solid rgba(255, 255, 255, 0.06)',

    /* Inner shadows — light catching glass edges, contained within bounds */
    boxShadow: [
      'inset 0 1px 0 rgba(255, 255, 255, 0.07)',
      'inset 0 -1px 0 rgba(0, 0, 0, 0.22)',
      'inset 0 0 30px rgba(255, 255, 255, 0.015)',
      '0 4px 20px rgba(0, 0, 0, 0.28)',
    ].join(', '),

    /* Animation states */
    opacity: animating ? 1 : 0,
    transform: animating
      ? 'translateY(0) scale(1)'
      : 'translateY(-4px) scale(0.96)',
    transition: animating
      ? 'opacity 200ms ease-out, transform 200ms cubic-bezier(0.16, 1, 0.3, 1)'
      : 'none',
  };

  return createPortal(
    <div ref={tooltipRef} style={tooltipStyle}>
      {children}
    </div>,
    document.body,
  );
};
