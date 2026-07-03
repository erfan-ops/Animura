import React from 'react';
import { ColorPicker } from './ColorPicker';

// ── Helpers ──

function formatName(str: string): string {
  if (!str) return '';
  return str
    .split('-')
    .map((w) => w.charAt(0).toUpperCase() + w.slice(1))
    .join(' ');
}

// ── Types ──

interface SchemaNode {
  type?: string;
  min?: number;
  max?: number;
  step?: number;
  options?: (string | number)[];
  [key: string]: unknown;
}

interface SettingsControlProps {
  schemaObj: Record<string, SchemaNode>;
  settingsObj: Record<string, unknown>;
  path?: string[];
  onUpdate: (key: string, value: unknown, path: string[]) => void;
}

// ── Slider ──

const SliderControl: React.FC<{
  value: number;
  min: number;
  max: number;
  step: number;
  isFloat: boolean;
  onChange: (v: number) => void;
}> = ({ value, min, max, step, isFloat, onChange }) => {
  const pct = ((value - min) / (max - min)) * 100;
  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: 14, marginBottom: 6 }}>
      <div style={{ flex: 1, position: 'relative', height: 20, display: 'flex', alignItems: 'center' }}>
        <div
          style={{
            width: '100%',
            height: 6,
            borderRadius: 3,
            background: 'var(--bg-surface)',
            border: '1px solid rgba(255,255,255,0.06)',
            position: 'relative',
            overflow: 'hidden',
          }}
        >
          <div
            style={{
              width: `${pct}%`,
              height: '100%',
              borderRadius: 3,
              background: 'linear-gradient(90deg, #c03078, var(--accent))',
            }}
          />
        </div>
        <input
          type="range"
          min={min}
          max={max}
          step={step}
          value={value}
          onChange={(e) => onChange(parseFloat(e.target.value))}
          style={{
            position: 'absolute',
            inset: 0,
            width: '100%',
            opacity: 0,
            cursor: 'pointer',
            margin: 0,
          }}
        />
        <div
          style={{
            position: 'absolute',
            left: `calc(${pct}% - 10px)`,
            top: 0,
            width: 20,
            height: 20,
            borderRadius: '50%',
            background: 'var(--text-primary)',
            border: '2px solid var(--accent)',
            pointerEvents: 'none',
          }}
        />
      </div>
      <span style={{ minWidth: 42, textAlign: 'right', fontSize: 13, color: 'var(--text-secondary)' }}>
        {isFloat ? value.toFixed(2) : value}
      </span>
    </div>
  );
};

// ── Toggle ──

const ToggleControl: React.FC<{
  checked: boolean;
  onChange: (v: boolean) => void;
}> = ({ checked, onChange }) => {
  return (
    <button
      onClick={() => onChange(!checked)}
      aria-label="Toggle"
      style={{
        width: 44,
        height: 26,
        borderRadius: 13,
        background: checked ? 'var(--accent)' : 'var(--bg-surface)',
        border: checked ? '1px solid rgba(255,64,112,0.3)' : '1px solid rgba(255,255,255,0.08)',
        cursor: 'pointer',
        position: 'relative',
        transition: 'background 150ms',
        padding: 0,
        marginBottom: 6,
      }}
    >
      <div
        style={{
          width: 20,
          height: 20,
          borderRadius: '50%',
          background: checked ? 'var(--text-primary)' : '#564a6e',
          position: 'absolute',
          left: checked ? 21 : 3,
          top: 2,
          transition: 'left 150ms cubic-bezier(0.16, 1, 0.3, 1), background 150ms',
        }}
      />
    </button>
  );
};

// ── Select ──

const SelectControl: React.FC<{
  value: string | number;
  options: (string | number)[];
  onChange: (v: string | number) => void;
}> = ({ value, options, onChange }) => {
  const [open, setOpen] = React.useState(false);
  const [hovered, setHovered] = React.useState(false);
  const [hoveredIdx, setHoveredIdx] = React.useState<number>(-1);

  return (
    <div style={{ position: 'relative', marginBottom: 6 }}>
      <button
        onClick={() => setOpen(!open)}
        onMouseEnter={() => setHovered(true)}
        onMouseLeave={() => setHovered(false)}
        style={{
          width: '100%',
          height: 38,
          borderRadius: 10,
          background: hovered ? 'var(--bg-surface-hover)' : 'var(--bg-surface)',
          border: hovered
            ? '1px solid rgba(224,64,144,0.3)'
            : '1px solid rgba(255,255,255,0.06)',
          color: 'var(--text-primary)',
          fontSize: 13,
          textAlign: 'left',
          padding: '0 14px',
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          transition: 'background 120ms, border-color 120ms',
          fontFamily: 'inherit',
        }}
      >
        <span>{String(value)}</span>
        <span style={{ color: 'var(--accent)', fontSize: 10 }}>▼</span>
      </button>
      {open && (
        <>
          <div
            style={{ position: 'fixed', inset: 0, zIndex: 9 }}
            onClick={() => setOpen(false)}
          />
          <div
            style={{
              position: 'absolute',
              top: 42,
              left: 0,
              right: 0,
              zIndex: 10,
              background: 'var(--bg-surface)',
              border: '1px solid rgba(224,64,144,0.2)',
              borderRadius: 12,
              padding: 4,
              maxHeight: 200,
              overflow: 'auto',
            }}
          >
            {options.map((opt, i) => {
              const isSelected = opt === value;
              const isHovered = hoveredIdx === i;
              return (
                <div
                  key={i}
                  onClick={() => {
                    onChange(opt);
                    setOpen(false);
                  }}
                  onMouseEnter={() => setHoveredIdx(i)}
                  onMouseLeave={() => setHoveredIdx(-1)}
                  style={{
                    padding: '8px 14px',
                    fontSize: 13,
                    color: isSelected
                      ? 'var(--accent)'
                      : isHovered
                        ? 'var(--text-primary)'
                        : 'var(--text-secondary)',
                    borderRadius: 6,
                    cursor: 'pointer',
                    background: isSelected
                      ? 'var(--bg-surface-hover)'
                      : isHovered
                        ? 'rgba(224,64,144,0.08)'
                        : 'transparent',
                    transition: 'background 100ms, color 100ms',
                  }}
                >
                  {String(opt)}
                </div>
              );
            })}
          </div>
        </>
      )}
    </div>
  );
};

// ── Color ──

const ColorControl: React.FC<{
  value: number[];
  onChange: (v: number[]) => void;
}> = ({ value, onChange }) => {
  return <ColorPicker value={value} onChange={onChange} />;
};

// ── Color List ──

const ColorListControl: React.FC<{
  colors: number[][];
  onChange: (v: number[][]) => void;
}> = ({ colors, onChange }) => {
  const [selectedIndex, setSelectedIndex] = React.useState<number>(-1);
  const [addBtnHovered, setAddBtnHovered] = React.useState(false);
  const [addBtnPressed, setAddBtnPressed] = React.useState(false);
  const [removeBtnHovered, setRemoveBtnHovered] = React.useState(false);
  const [removeBtnPressed, setRemoveBtnPressed] = React.useState(false);

  const addColor = () => {
    // Add a default white color; user can click to open the picker and adjust.
    const newColors = [...colors, [1, 1, 1, 1]];
    onChange(newColors);
    setSelectedIndex(newColors.length - 1);
  };

  const removeColor = () => {
    if (selectedIndex < 0) return;
    const newColors = colors.filter((_, i) => i !== selectedIndex);
    onChange(newColors);
    setSelectedIndex(Math.min(selectedIndex, newColors.length - 1));
  };

  return (
    <div style={{ marginBottom: 8 }}>
      <div style={{ display: 'flex', gap: 8, marginBottom: 8 }}>
        <button
          onMouseEnter={() => setAddBtnHovered(true)}
          onMouseLeave={() => {setAddBtnHovered(false); setAddBtnPressed(false)}}
          onMouseDown={() => setAddBtnPressed(true)}
          onMouseUp={() => setAddBtnPressed(false)}
          onClick={addColor}
          style={{
            width: 36,
            height: 36,
            borderRadius: 8,
            background: addBtnHovered? 'var(--bg-surface-hover)' : 'var(--bg-surface)',
            border: '1px solid rgba(255,255,255,0.06)',
            color: addBtnPressed ? 'var(--accent)' : 'var(--text-primary)',
            fontSize: 18,
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            fontFamily: 'inherit',
            textShadow: addBtnPressed ? '0 0 4px var(--accent)' : '0 0 0px var(--text-primary)'
          }}
        >
          +
        </button>
        <button
          onClick={removeColor}
          onMouseEnter={() => setRemoveBtnHovered(true)}
          onMouseLeave={() => {setRemoveBtnHovered(false); setRemoveBtnPressed(false)}}
          onMouseDown={() => setRemoveBtnPressed(true)}
          onMouseUp={() => setRemoveBtnPressed(false)}
          disabled={selectedIndex < 0}
          style={{
            width: 36,
            height: 36,
            borderRadius: 8,
            background: removeBtnHovered? 'var(--bg-surface-hover)' : 'var(--bg-surface)',
            border: '1px solid rgba(255,255,255,0.06)',
            color: removeBtnPressed ? 'var(--accent)' : 'var(--text-primary)',
            fontSize: 20,
            cursor: selectedIndex >= 0 ? 'pointer' : 'default',
            opacity: selectedIndex >= 0 ? 1 : 0.4,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            fontFamily: 'inherit',
            textShadow: removeBtnPressed ? '0 0 4px var(--accent)' : 'none'
          }}
        >
          −
        </button>
      </div>
      <div
        style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, 46px)',
          gap: 4,
          maxHeight: 154,
          overflow: 'auto',
        }}
      >
        {colors.map((c, i) => (
          <div
            key={i}
            onClick={() => setSelectedIndex(i)}
            style={{
              lineHeight: 0,
              padding: 2, // give the selected box-shadow ring room to render without clipping
            }}
          >
            <ColorPicker
              compact={true}
              selected={i === selectedIndex}
              value={c}
              onChange={(v) => {
                const newColors = [...colors];
                newColors[i] = v;
                onChange(newColors);
              }}
            />
          </div>
        ))}
      </div>
    </div>
  );
};

// ── Main SettingsControl (recursive) ──

export const SettingsControl: React.FC<SettingsControlProps> = ({
  schemaObj,
  settingsObj,
  path = [],
  onUpdate,
}) => {
  const keys = Object.keys(schemaObj);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
      {keys.map((key) => {
        const fieldSchema = schemaObj[key];
        const fieldType = fieldSchema.type || 'group';

        // Resolve value through path
        let current: Record<string, unknown> = settingsObj;
        for (const p of path) {
          if (current && typeof current[p] === 'object' && current[p] !== null) {
            current = current[p] as Record<string, unknown>;
          } else {
            current = {};
            break;
          }
        }

        // ── Label ──
        const labelEl = (
          <div
            style={{
              fontSize: fieldType === 'group' ? 16 : 13,
              fontWeight: fieldType === 'group' ? 600 : 500,
              color: fieldType === 'group' ? 'var(--accent)' : 'var(--text-primary)',
              marginTop: fieldType === 'group' ? 16 : 0,
              marginBottom: fieldType === 'group' ? 0 : 4,
            }}
          >
            {formatName(key)}
            {fieldType === 'group' && (
              <div
                style={{
                  height: 1,
                  background: 'rgba(224,64,144,0.15)',
                  marginTop: 3,
                }}
              />
            )}
          </div>
        );

        // ── Control ──
        let controlEl: React.ReactNode = null;

        switch (fieldType) {
          case 'int':
          case 'float': {
            const val = (current[key] as number) ?? 0;
            const min = fieldSchema.min ?? 0;
            const max = fieldSchema.max ?? 100;
            const step = fieldSchema.step ?? (fieldType === 'int' ? 1 : 0.01);
            controlEl = (
              <SliderControl
                value={val}
                min={min}
                max={max}
                step={step}
                isFloat={fieldType === 'float'}
                onChange={(v) => onUpdate(key, v, path)}
              />
            );
            break;
          }

          case 'bool': {
            const val = (current[key] as boolean) ?? false;
            controlEl = (
              <ToggleControl
                checked={val}
                onChange={(v) => onUpdate(key, v, path)}
              />
            );
            break;
          }

          case 'select': {
            const val = (current[key] as string | number) ?? '';
            const opts = (fieldSchema.options as (string | number)[]) || [];
            controlEl = (
              <SelectControl
                value={val}
                options={opts}
                onChange={(v) => onUpdate(key, v, path)}
              />
            );
            break;
          }

          case 'color': {
            const val = (current[key] as number[]) || [1, 1, 1, 1];
            controlEl = (
              <ColorControl
                value={val}
                onChange={(v) => onUpdate(key, v, path)}
              />
            );
            break;
          }

          case 'color_list': {
            const val = (current[key] as number[][]) || [];
            controlEl = (
              <ColorListControl
                colors={val}
                onChange={(v) => onUpdate(key, v, path)}
              />
            );
            break;
          }

          case 'group': {
            controlEl = (
              <div style={{ display: 'flex', gap: 14 }}>
                <div
                  style={{
                    width: 2,
                    background: 'var(--accent)',
                    opacity: 0.4,
                    borderRadius: 1,
                    flexShrink: 0,
                  }}
                />
                <div style={{ flex: 1 }}>
                  <SettingsControl
                    schemaObj={fieldSchema as Record<string, SchemaNode>}
                    settingsObj={settingsObj}
                    path={[...path, key]}
                    onUpdate={onUpdate}
                  />
                </div>
              </div>
            );
            break;
          }

          default:
            break;
        }

        return (
          <div key={key}>
            {labelEl}
            {controlEl}
          </div>
        );
      })}
    </div>
  );
};
