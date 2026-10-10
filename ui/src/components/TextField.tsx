import { useId } from 'react';

export interface TextFieldProps {
  label: string;
  value: string;
  onChange: (value: string) => void;
  type?: 'text' | 'password';
  /** Tells the browser what the field is, so it can fill it in or offer to save it. */
  autoComplete?: string;
  /** Which on-screen keyboard a phone shows. */
  inputMode?: 'text' | 'decimal';
  placeholder?: string;
  hint?: string;
  /** Shown in place of the hint, in the warning colour. */
  error?: string | null;
  disabled?: boolean;
  maxLength?: number;
}

export function TextField({
  label,
  value,
  onChange,
  type = 'text',
  autoComplete = 'off',
  inputMode,
  placeholder,
  hint,
  error,
  disabled = false,
  maxLength,
}: TextFieldProps) {
  const id = useId();
  const noteId = `${id}-note`;
  const note = error ?? hint;

  return (
    <div className="field">
      <div className="field-head">
        <label htmlFor={id}>{label}</label>
      </div>
      <input
        id={id}
        className={error ? 'text invalid' : 'text'}
        type={type}
        value={value}
        autoComplete={autoComplete}
        autoCapitalize="none"
        autoCorrect="off"
        spellCheck={false}
        inputMode={inputMode}
        placeholder={placeholder}
        maxLength={maxLength}
        disabled={disabled}
        aria-invalid={error ? true : undefined}
        aria-describedby={note ? noteId : undefined}
        onChange={(event) => onChange(event.target.value)}
      />
      {note && (
        <p id={noteId} className={error ? 'hint error-text' : 'hint'}>
          {note}
        </p>
      )}
    </div>
  );
}
