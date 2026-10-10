import { useId } from 'react';
import type { CSSProperties } from 'react';

export interface SliderProps {
  label: string;
  value: number;
  min: number;
  max: number;
  step?: number;
  /**
   * "fine-low" spreads the low end of the range over more of the track. Used
   * for times, where 20 ms to 200 ms matters more than 1800 ms to 2000 ms.
   */
  scale?: 'linear' | 'fine-low';
  /** Puts the minimum at the right-hand end, for "further right is faster". */
  reversed?: boolean;
  format?: (value: number) => string;
  hint?: string;
  disabled?: boolean;
  onChange: (value: number) => void;
}

const TRACK_STEPS = 1000;

function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value));
}

/** Where on the track (0..1) a value sits. */
function toFraction(value: number, min: number, max: number, scale: SliderProps['scale']): number {
  const linear = max === min ? 0 : (clamp(value, min, max) - min) / (max - min);
  return scale === 'fine-low' ? Math.sqrt(linear) : linear;
}

/** The value at a point (0..1) on the track, rounded to the step. */
function fromFraction(fraction: number, min: number, max: number, step: number, scale: SliderProps['scale']): number {
  const linear = scale === 'fine-low' ? fraction * fraction : fraction;
  const raw = min + (max - min) * linear;
  return clamp(Math.round(raw / step) * step, min, max);
}

export function Slider({
  label,
  value,
  min,
  max,
  step = 1,
  scale = 'linear',
  reversed = false,
  format = String,
  hint,
  disabled = false,
  onChange,
}: SliderProps) {
  const id = useId();
  const fraction = toFraction(value, min, max, scale);
  const position = Math.round((reversed ? 1 - fraction : fraction) * TRACK_STEPS);

  return (
    <div className="field">
      <div className="field-head">
        <label htmlFor={id}>{label}</label>
        <output htmlFor={id}>{format(value)}</output>
      </div>
      <input
        id={id}
        type="range"
        min={0}
        max={TRACK_STEPS}
        step={1}
        value={position}
        disabled={disabled}
        aria-valuetext={format(value)}
        style={{ '--fill': `${(position / TRACK_STEPS) * 100}%` } as CSSProperties}
        onChange={(event) => {
          const at = Number(event.target.value) / TRACK_STEPS;
          onChange(fromFraction(reversed ? 1 - at : at, min, max, step, scale));
        }}
      />
      {hint !== undefined && <p className="hint">{hint}</p>}
    </div>
  );
}
