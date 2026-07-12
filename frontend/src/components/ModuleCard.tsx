/**
 * @file components/ModuleCard.tsx
 * @brief Neumorphic preview card for a wallpaper module in the grid.
 *
 * Each card is 260×220px with a layered neumorphic design:
 * - **Shadow layer** — offset dark rectangle behind the card for depth
 *   (no GPU-heavy box-shadow).
 * - **Surface layer** — gradient purple surface with subtle white border.
 * - **Preview image** — loaded from the WebView2 virtual host at
 *   `https://animura.modules/<folder>/<file>`. Uses contain-quality scaling
 *   with 120% oversize + overflow clip to fill the image area.
 * - **Name pill** — translucent dark pill at the bottom with the module name.
 *
 * ## Hover Effects
 * - Card scales to 1.04× with a cubic-bezier transition.
 * - Surface gradient lightens, border turns pink (`rgba(224,64,144,0.4)`).
 * - Pink glow shadow appears around the card.
 * - A glassmorphism tooltip fades in above/below the card after a short
 *   hover delay, showing the module name, version, and description.
 */

import React from 'react';
import type { ModuleInfo } from '../types';
import { Tooltip } from './Tooltip';

interface ModuleCardProps {
  module: ModuleInfo;
  onClick: () => void;
}

export const ModuleCard: React.FC<ModuleCardProps> = ({ module, onClick }) => {
  const [hovered, setHovered] = React.useState(false);
  const [tooltipVisible, setTooltipVisible] = React.useState(false);
  const cardRef = React.useRef<HTMLDivElement>(null);
  const showTimerRef = React.useRef<number>(0);
  const hideTimerRef = React.useRef<number>(0);

  /** Show tooltip after a brief hover delay (avoids flicker on fast passes). */
  const handleMouseEnter = React.useCallback(() => {
    setHovered(true);
    clearTimeout(hideTimerRef.current);
    showTimerRef.current = window.setTimeout(
      () => setTooltipVisible(true),
      450,
    );
  }, []);

  /** Hide tooltip immediately; card hover state resets. */
  const handleMouseLeave = React.useCallback(() => {
    setHovered(false);
    clearTimeout(showTimerRef.current);
    setTooltipVisible(false);
  }, []);

  /* Clean up timers on unmount. */
  React.useEffect(() => {
    return () => {
      clearTimeout(showTimerRef.current);
      clearTimeout(hideTimerRef.current);
    };
  }, []);

  return (
    <>
      <div
        ref={cardRef}
        onClick={onClick}
        onMouseEnter={handleMouseEnter}
        onMouseLeave={handleMouseLeave}
        style={{
          width: 260,
          height: 220,
          position: 'relative',
          cursor: 'pointer',
          transform: hovered ? 'scale(1.04)' : 'scale(1)',
          transition: 'transform 200ms cubic-bezier(0.16, 1, 0.3, 1)',
        }}
      >
      {/* Shadow layer (neumorphic depth) */}
      <div
        style={{
          position: 'absolute',
          inset: 0,
          borderRadius: 'var(--radius-card)',
          background: 'var(--shadow-dark)',
          opacity: hovered ? 0.7 : 0.5,
          transform: `translate(${hovered ? 5 : 3}px, ${hovered ? 6 : 4}px)`,
          transition: 'opacity 200ms, transform 200ms',
        }}
      />

      {/* Card surface */}
      <div
        style={{
          position: 'absolute',
          inset: 0,
          borderRadius: 'var(--radius-card)',
          background: hovered
            ? 'linear-gradient(180deg, #281d48 0%, #1e1438 100%)'
            : 'linear-gradient(180deg, #1e1438 0%, #160d2a 100%)',
          border: `1px solid ${hovered ? 'rgba(224,64,144,0.4)' : 'rgba(255,255,255,0.03)'}`,
          transition:
            'background 180ms, border-color 180ms, border-width 180ms',
          overflow: 'hidden',
          display: 'flex',
          flexDirection: 'column',
          boxShadow: `0 0 ${hovered ? '10' : '0'}px rgba(224,64,144,0.7)`,
        }}
      >
        {/* Preview image — contain for quality, oversized to fill, clipped by overflow */}
        <div
          style={{
            flex: 1,
            margin: 4,
            marginBottom: 2,
            borderRadius: 14,
            overflow: 'hidden',
            background: 'var(--bg-base)',
            border: `1px solid ${hovered ? 'rgba(224,64,144,0.4)' : 'rgba(0,0,0,0.3)'}`,
            position: 'relative',
          }}
        >
          <img
            src={module.previewPath}
            alt={module.name}
            style={{
              position: 'absolute',
              top: '50%',
              left: '50%',
              width: '120%',
              height: '120%',
              objectFit: 'contain',
              objectPosition: 'center center',
              transform: 'translate(-50%, -50%)',
              opacity: hovered ? 1 : 0.9,
            }}
            onError={(e) => {
              (e.target as HTMLImageElement).style.display = 'none';
            }}
          />
        </div>

        {/* Module name pill */}
        <div
          style={{
            height: 36,
            margin: '4px 8px 8px',
            borderRadius: 12,
            background: 'rgba(6, 3, 16, 0.75)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
          }}
        >
          <span
            style={{
              fontSize: 13,
              fontWeight: 500,
              color: 'var(--text-primary)',
              overflow: 'hidden',
              textOverflow: 'ellipsis',
              whiteSpace: 'nowrap',
              maxWidth: 'calc(100% - 16px)',
            }}
          >
            {module.name}
          </span>
        </div>
      </div>
    </div>

    {/* Glassmorphism tooltip — module name, version, description */}
    <Tooltip targetRef={cardRef} visible={tooltipVisible}>
      <div
        style={{
          display: 'flex',
          flexDirection: 'column',
          gap: 6,
        }}
      >
        {/* Module name + version badge */}
        <div
          style={{
            display: 'flex',
            alignItems: 'baseline',
            gap: 8,
          }}
        >
          <span
            style={{
              fontSize: 14,
              fontWeight: 600,
              color: 'var(--text-primary)',
              lineHeight: 1.3,
            }}
          >
            {module.name}
          </span>
          <span
            style={{
              fontSize: 11,
              fontWeight: 400,
              color: 'var(--text-secondary)',
              opacity: 0.7,
            }}
          >
            v{module.version}
          </span>
        </div>

        {/* Description */}
        <span
          style={{
            fontSize: 12,
            fontWeight: 400,
            color: 'var(--text-secondary)',
            lineHeight: 1.5,
            opacity: 0.85,
          }}
        >
          {module.description}
        </span>
      </div>
    </Tooltip>
  </>
  );
};
