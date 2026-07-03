import React, { useState, useRef, useCallback, useEffect } from 'react';
import { createPortal } from 'react-dom';

// ── Color conversion utilities ──

function clamp(v: number, lo: number, hi: number): number {
  return v < lo ? lo : v > hi ? hi : v;
}

function rgbaToHsva(r: number, g: number, b: number, a: number): [number, number, number, number] {
  const max = Math.max(r, g, b);
  const min = Math.min(r, g, b);
  const d = max - min;
  const v = max;
  const s = max === 0 ? 0 : d / max;
  let h = 0;
  if (d !== 0) {
    if (max === r) h = ((g - b) / d + (g < b ? 6 : 0)) / 6;
    else if (max === g) h = ((b - r) / d + 2) / 6;
    else h = ((r - g) / d + 4) / 6;
  }
  return [h * 360, s, v, a];
}

function hsvaToRgba(h: number, s: number, v: number, a: number): number[] {
  h = ((h % 360) + 360) % 360;
  const c = v * s;
  const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
  const m = v - c;
  let rp = 0, gp = 0, bp = 0;
  if (h < 60) { rp = c; gp = x; }
  else if (h < 120) { rp = x; gp = c; }
  else if (h < 180) { gp = c; bp = x; }
  else if (h < 240) { gp = x; bp = c; }
  else if (h < 300) { rp = x; bp = c; }
  else { rp = c; bp = x; }
  return [
    clamp(rp + m, 0, 1),
    clamp(gp + m, 0, 1),
    clamp(bp + m, 0, 1),
    clamp(a, 0, 1),
  ];
}

function rgbaToCss(rgba: number[]): string {
  const r = Math.round(rgba[0] * 255);
  const g = Math.round(rgba[1] * 255);
  const b = Math.round(rgba[2] * 255);
  const a = rgba.length > 3 ? rgba[3] : 1;
  return `rgba(${r},${g},${b},${a})`;
}

function hsvaToHex(h: number, s: number, v: number, a: number): string {
  const [r, g, b] = hsvaToRgba(h, s, v, 1);
  const toHex = (n: number) => Math.round(n * 255).toString(16).padStart(2, '0');
  const alpha = Math.round(a * 255).toString(16).padStart(2, '0');
  return `#${toHex(r)}${toHex(g)}${toHex(b)}${alpha === 'ff' ? '' : alpha}`;
}

function hexToRgba(hex: string): number[] {
  const h = hex.replace('#', '');
  const r = parseInt(h.slice(0, 2), 16) / 255;
  const g = parseInt(h.slice(2, 4), 16) / 255;
  const b = parseInt(h.slice(4, 6), 16) / 255;
  const a = h.length >= 8 ? parseInt(h.slice(6, 8), 16) / 255 : 1;
  return [r, g, b, a];
}

// ── EyeDropper API (Chromium / WebView2) ──

interface EyeDropperResult {
  sRGBHex: string;
}
interface EyeDropper {
  open(options?: { signal?: AbortSignal }): Promise<EyeDropperResult>;
}
declare global {
  interface Window {
    EyeDropper?: new () => EyeDropper;
  }
}

// ── Preset palette ──

const PRESET_COLORS: number[][] = [
  [1, 0, 0, 1],          [1, 0.27, 0, 1],       [1, 0.55, 0, 1],
  [1, 0.84, 0, 1],       [0.95, 0.95, 0, 1],    [0.68, 1, 0.18, 1],
  [0, 1, 0, 1],          [0, 0.98, 0.6, 1],     [0, 1, 1, 1],
  [0.12, 0.56, 1, 1],    [0, 0, 1, 1],          [0.54, 0.17, 0.89, 1],
  [0.58, 0, 0.83, 1],    [1, 0, 1, 1],          [1, 0.08, 0.58, 1],
  [0, 0, 0, 1],          [0.3, 0.3, 0.3, 1],    [0.6, 0.6, 0.6, 1],
  [0.9, 0.9, 0.9, 1],    [1, 1, 1, 1],
];
const MAX_CUSTOM = 8;
const CUSTOM_STORAGE_KEY = 'animura-custom-colors';

function loadCustomColors(): number[][] {
  try {
    const raw = localStorage.getItem(CUSTOM_STORAGE_KEY);
    if (raw) return JSON.parse(raw) as number[][];
  } catch { /* ignore */ }
  return [];
}
function saveCustomColors(colors: number[][]) {
  try { localStorage.setItem(CUSTOM_STORAGE_KEY, JSON.stringify(colors)); } catch { /* ignore */ }
}

// ── Reusable slider thumb ──

const thumbStyle: React.CSSProperties = {
  position: 'absolute',
  width: 16, height: 16,
  borderRadius: '50%',
  border: '2px solid #ede4f5',
  boxShadow: '0 0 4px rgba(0,0,0,0.5), inset 0 0 0 1px rgba(0,0,0,0.2)',
  pointerEvents: 'none',
  transform: 'translate(-50%, -50%)',
  zIndex: 2,
};

// ── Shared checkerboard pattern ──

const checkerboard = {
  backgroundImage: `linear-gradient(45deg, #555 25%, transparent 25%),
    linear-gradient(-45deg, #555 25%, transparent 25%),
    linear-gradient(45deg, transparent 75%, #555 75%),
    linear-gradient(-45deg, transparent 75%, #555 75%)`,
  backgroundSize: '8px 8px',
  backgroundPosition: '0 0, 0 4px, 4px -4px, -4px 0',
};

// ── Component ──

interface ColorPickerProps {
  value: number[];
  onChange: (v: number[]) => void;
  /** Renders a compact square trigger for use in color-list grids. */
  compact?: boolean;
  /** When true, shows an accent ring around the swatch (for color-list selection). */
  selected?: boolean;
}

export const ColorPicker: React.FC<ColorPickerProps> = ({ value, onChange, compact = false, selected = false }) => {
  const [open, setOpen] = useState(false);
  const [hsva, setHsva] = useState<[number, number, number, number]>(
    () => rgbaToHsva(value[0], value[1], value[2], value[3] ?? 1),
  );
  const [customColors, setCustomColors] = useState<number[][]>(loadCustomColors);
  const [drag, setDrag] = useState<'area' | 'hue' | 'alpha' | null>(null);

  const areaRef = useRef<HTMLDivElement>(null);
  const hueRef = useRef<HTMLDivElement>(null);
  const alphaRef = useRef<HTMLDivElement>(null);

  // Sync when external value changes
  useEffect(() => {
    setHsva(rgbaToHsva(value[0], value[1], value[2], value[3] ?? 1));
  }, [value]);

  const commit = useCallback(
    (h: number, s: number, v: number, a: number) => {
      const next = hsvaToRgba(h, s, v, a);
      onChange(next);
    },
    [onChange],
  );

  // ── Eye dropper ──

  const hasEyeDropper = typeof window !== 'undefined' && 'EyeDropper' in window;

  const openEyeDropper = useCallback(async () => {
    if (!hasEyeDropper) return;
    try {
      const dropper = new window.EyeDropper!();
      const result = await dropper.open();
      const rgba = hexToRgba(result.sRGBHex);
      const next = rgbaToHsva(rgba[0], rgba[1], rgba[2], rgba[3]);
      setHsva(next);
      commit(next[0], next[1], next[2], next[3]);
    } catch {
      // User cancelled (Escape) — ignore
    }
  }, [hasEyeDropper, commit]);

  // ── Pointer tracking ──

  const onPointerDown = useCallback(
    (kind: 'area' | 'hue' | 'alpha') =>
    (e: React.PointerEvent) => {
      (e.target as HTMLElement).setPointerCapture(e.pointerId);
      setDrag(kind);
    },
    [],
  );

  useEffect(() => {
    if (!drag) return;
    const onMove = (e: PointerEvent) => {
      const [h, s, v, a] = hsva;
      if (drag === 'area' && areaRef.current) {
        const rect = areaRef.current.getBoundingClientRect();
        const x = clamp((e.clientX - rect.left) / rect.width, 0, 1);
        const y = clamp(1 - (e.clientY - rect.top) / rect.height, 0, 1);
        const next: [number, number, number, number] = [h, x, y, a];
        setHsva(next);
        commit(h, x, y, a);
      } else if (drag === 'hue' && hueRef.current) {
        const rect = hueRef.current.getBoundingClientRect();
        const nh = clamp((e.clientX - rect.left) / rect.width, 0, 1) * 360;
        const next: [number, number, number, number] = [nh, s, v, a];
        setHsva(next);
        commit(nh, s, v, a);
      } else if (drag === 'alpha' && alphaRef.current) {
        const rect = alphaRef.current.getBoundingClientRect();
        const na = clamp((e.clientX - rect.left) / rect.width, 0, 1);
        const next: [number, number, number, number] = [h, s, v, na];
        setHsva(next);
        commit(h, s, v, na);
      }
    };
    const onUp = () => setDrag(null);
    window.addEventListener('pointermove', onMove);
    window.addEventListener('pointerup', onUp);
    return () => {
      window.removeEventListener('pointermove', onMove);
      window.removeEventListener('pointerup', onUp);
    };
  }, [drag, hsva, commit]);

  // ── Add / remove custom colors ──

  const addCustom = useCallback(() => {
    setCustomColors((prev) => {
      if (prev.length >= MAX_CUSTOM) return prev;
      const next = [...prev, [...value]];
      saveCustomColors(next);
      return next;
    });
  }, [value]);

  const removeCustom = useCallback(
    (idx: number) => {
      setCustomColors((prev) => {
        const next = prev.filter((_, i) => i !== idx);
        saveCustomColors(next);
        return next;
      });
    },
    [],
  );

  const [h, s, v, a] = hsva;
  const hueColor = rgbaToCss(hsvaToRgba(h, 1, 1, 1));
  const currentRgba = hsvaToRgba(h, s, v, a);
  const currentCss = rgbaToCss(currentRgba);
  const hex = hsvaToHex(h, s, v, a);

  // Trigger dimensions: normal = 48×36, compact = 36×36 square
  const triggerW = compact ? 36 : 48;
  const triggerH = compact ? 36 : 36;
  const triggerRadius = compact ? 8 : 10;

  return (
    <>
      {/* Swatch trigger */}
      <div
        onClick={() => setOpen(true)}
        title={hex}
        style={{
          width: triggerW, height: triggerH, borderRadius: triggerRadius,
          backgroundColor: currentCss,
          border: '1.5px solid rgba(255,255,255,0.12)',
          boxShadow: selected
            ? 'inset 0 0 0 1px rgba(0,0,0,0.25), 0 0 0 2px var(--accent)'
            : 'inset 0 0 0 1px rgba(0,0,0,0.25)',
          cursor: 'pointer', position: 'relative',
          marginBottom: compact ? 0 : 4,
          // Checkerboard for alpha visibility
          ...(a < 1 ? checkerboard : {}),
        }}
      >
        <div style={{
          position: 'absolute', inset: 0, borderRadius: triggerRadius,
          backgroundColor: currentCss,
        }} />
      </div>

      {/* Popup — portaled to body to escape SettingsPanel's transform+overflow clipping */}
      {open && createPortal(
        <>
          {/* Backdrop */}
          <div
            onClick={() => setOpen(false)}
            style={{ position: 'fixed', inset: 0, zIndex: 9998 }}
          />

          {/* Picker panel */}
          <div
            style={{
              position: 'fixed', zIndex: 9999,
              top: '50%', left: '50%',
              transform: 'translate(-50%, -50%)',
              width: 288,
              background: '#1a1035',
              border: '1px solid rgba(224,64,144,0.25)',
              borderRadius: 16,
              padding: 18,
              display: 'flex', flexDirection: 'column', gap: 12,
              boxShadow: '0 8px 40px rgba(0,0,0,0.6)',
            }}
            onClick={(e) => e.stopPropagation()}
          >
            {/* ── Color area (SV) ── */}
            <div
              ref={areaRef}
              onPointerDown={onPointerDown('area')}
              style={{
                width: '100%', height: 150, borderRadius: 10,
                background: hueColor,
                position: 'relative', cursor: 'crosshair', touchAction: 'none',
                overflow: 'hidden',
              }}
            >
              <div style={{
                position: 'absolute', inset: 0,
                background: 'linear-gradient(to right, #fff, transparent)',
              }} />
              <div style={{
                position: 'absolute', inset: 0,
                background: 'linear-gradient(to top, #000, transparent)',
              }} />
              <div style={{
                ...thumbStyle,
                left: `${s * 100}%`, top: `${(1 - v) * 100}%`,
                background: currentCss,
                borderColor: v > 0.5 ? '#0d0620' : '#ede4f5',
              }} />
            </div>

            {/* ── Hue slider ── */}
            <div
              ref={hueRef}
              onPointerDown={onPointerDown('hue')}
              style={{
                width: '100%', height: 14, borderRadius: 7,
                background: 'linear-gradient(to right, #f00, #ff0, #0f0, #0ff, #00f, #f0f, #f00)',
                position: 'relative', cursor: 'pointer', touchAction: 'none',
              }}
            >
              <div style={{
                ...thumbStyle,
                left: `${(h / 360) * 100}%`, top: '50%',
                background: rgbaToCss(hsvaToRgba(h, 1, 1, 1)),
              }} />
            </div>

            {/* ── Alpha slider ── */}
            <div
              ref={alphaRef}
              onPointerDown={onPointerDown('alpha')}
              style={{
                width: '100%', height: 14, borderRadius: 7,
                position: 'relative', cursor: 'pointer', touchAction: 'none',
                overflow: 'hidden',
                ...checkerboard,
              }}
            >
              <div style={{
                position: 'absolute', inset: 0,
                background: `linear-gradient(to right, transparent, ${rgbaToCss(hsvaToRgba(h, s, v, 1))})`,
              }} />
              <div style={{
                ...thumbStyle,
                left: `${a * 100}%`, top: '50%',
                background: currentCss,
              }} />
            </div>

            {/* ── Preview + hex + eye dropper ── */}
            <div style={{
              display: 'flex', alignItems: 'center', gap: 10,
            }}>
              <div style={{
                width: 36, height: 36, borderRadius: 8,
                border: '1px solid rgba(255,255,255,0.1)',
                flexShrink: 0, position: 'relative',
                overflow: 'hidden',
                ...(a < 1 ? checkerboard : {}),
              }}>
                <div style={{
                  width: '100%', height: '100%',
                  backgroundColor: currentCss,
                }} />
              </div>
              <span style={{
                fontSize: 14, fontFamily: 'monospace',
                color: 'var(--text-primary)',
                userSelect: 'text', flex: 1,
              }}>
                {hex}
              </span>
              {hasEyeDropper && (
                <button
                  onClick={openEyeDropper}
                  title="Pick a color from your screen"
                  style={{
                    width: 30, height: 30, borderRadius: 8,
                    background: 'var(--bg-surface)',
                    border: '1px solid rgba(255,255,255,0.08)',
                    color: 'var(--text-secondary)',
                    fontSize: 14, cursor: 'pointer',
                    display: 'flex', alignItems: 'center', justifyContent: 'center',
                    fontFamily: 'inherit', flexShrink: 0,
                  }}
                >
                  <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                    <path d="M12 2a2 2 0 0 1 2 2v4l6 6a2 2 0 0 1 0 2.83l-2.83 2.83a2 2 0 0 1-2.83 0L8 14H4a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h4a2 2 0 0 0 2-2V4a2 2 0 0 1 2-2z"/>
                    <circle cx="16" cy="16" r="6" fill="currentColor" stroke="none" opacity="0.3"/>
                    <path d="M16 13v6M13 16h6" />
                  </svg>
                </button>
              )}
            </div>

            {/* ── Preset colors ── */}
            <div>
              <div style={{
                fontSize: 12, fontWeight: 500,
                color: 'var(--text-secondary)', marginBottom: 6,
              }}>
                Presets
              </div>
              <div style={{
                display: 'grid',
                gridTemplateColumns: 'repeat(10, 1fr)',
                gap: 4,
              }}>
                {PRESET_COLORS.map((c, i) => (
                  <div
                    key={i}
                    onClick={() => {
                      const next = rgbaToHsva(c[0], c[1], c[2], c[3]);
                      setHsva(next);
                      commit(next[0], next[1], next[2], next[3]);
                    }}
                    title={hsvaToHex(
                      ...rgbaToHsva(c[0], c[1], c[2], c[3]),
                    )}
                    style={{
                      aspectRatio: '1', borderRadius: 6,
                      background: rgbaToCss(c),
                      border: '1px solid rgba(255,255,255,0.1)',
                      cursor: 'pointer',
                      transition: 'transform 100ms',
                      boxSizing: 'border-box',
                    }}
                    onMouseEnter={(e) => {
                      (e.target as HTMLElement).style.transform = 'scale(1.2)';
                    }}
                    onMouseLeave={(e) => {
                      (e.target as HTMLElement).style.transform = 'scale(1)';
                    }}
                  />
                ))}
              </div>
            </div>

            {/* ── Custom colors ── */}
            <div>
              <div style={{
                display: 'flex', justifyContent: 'space-between', alignItems: 'center',
                marginBottom: 6,
              }}>
                <span style={{ fontSize: 12, fontWeight: 500, color: 'var(--text-secondary)' }}>
                  Custom ({customColors.length}/{MAX_CUSTOM})
                </span>
                <button
                  onClick={addCustom}
                  disabled={customColors.length >= MAX_CUSTOM}
                  style={{
                    background: 'var(--bg-surface)',
                    border: '1px solid rgba(255,255,255,0.06)',
                    color: customColors.length >= MAX_CUSTOM ? '#564a6e' : 'var(--text-primary)',
                    fontSize: 11, fontWeight: 500,
                    borderRadius: 6, padding: '2px 10px', cursor: 'pointer',
                    fontFamily: 'inherit',
                  }}
                >
                  + Add current
                </button>
              </div>
              {customColors.length === 0 ? (
                <div style={{ fontSize: 11, color: '#564a6e', fontStyle: 'italic' }}>
                  No custom colors yet. Click "+ Add current" to save the active color.
                </div>
              ) : (
                <div style={{
                  display: 'grid',
                  gridTemplateColumns: 'repeat(8, 1fr)',
                  gap: 4,
                }}>
                  {customColors.map((c, i) => (
                    <div
                      key={i}
                      onClick={() => {
                        const next = rgbaToHsva(c[0], c[1], c[2], c[3]);
                        setHsva(next);
                        commit(next[0], next[1], next[2], next[3]);
                      }}
                      onContextMenu={(e) => {
                        e.preventDefault();
                        removeCustom(i);
                      }}
                      title={`${rgbaToCss(c)} — right-click to remove`}
                      style={{
                        aspectRatio: '1', borderRadius: 6,
                        background: rgbaToCss(c),
                        border: '1px solid rgba(255,255,255,0.1)',
                        cursor: 'pointer',
                        transition: 'transform 100ms',
                        position: 'relative',
                        boxSizing: 'border-box',
                      }}
                      onMouseEnter={(e) => {
                        (e.target as HTMLElement).style.transform = 'scale(1.15)';
                      }}
                      onMouseLeave={(e) => {
                        (e.target as HTMLElement).style.transform = 'scale(1)';
                      }}
                    />
                  ))}
                </div>
              )}
            </div>
          </div>
        </>,
        document.body,
      )}
    </>
  );
};
