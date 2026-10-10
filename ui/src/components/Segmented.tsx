export interface SegmentedOption<T extends string> {
  value: T;
  label: string;
}

export interface SegmentedProps<T extends string> {
  label: string;
  value: T;
  options: readonly SegmentedOption<T>[];
  /** Lay the choices out two to a row; for labels too long to sit side by side. */
  twoColumns?: boolean;
  disabled?: boolean;
  onChange: (value: T) => void;
}

/** A row of mutually exclusive choices. */
export function Segmented<T extends string>({
  label,
  value,
  options,
  twoColumns = false,
  disabled = false,
  onChange,
}: SegmentedProps<T>) {
  return (
    <div className="field">
      <div className="field-head">
        <span>{label}</span>
      </div>
      <div className={twoColumns ? 'segmented two-columns' : 'segmented'} role="radiogroup" aria-label={label}>
        {options.map((option) => (
          <button
            key={option.value}
            type="button"
            role="radio"
            aria-checked={option.value === value}
            disabled={disabled}
            onClick={() => onChange(option.value)}
          >
            {option.label}
          </button>
        ))}
      </div>
    </div>
  );
}
