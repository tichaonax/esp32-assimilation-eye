import { useEffect, useState } from 'react';

export interface ConfirmButtonProps {
  label: string;
  confirmLabel: string;
  disabled?: boolean;
  onConfirm: () => void;
}

const ARMED_FOR_MS = 4000;

/** A button that must be tapped twice, so a stray touch does nothing. */
export function ConfirmButton({ label, confirmLabel, disabled = false, onConfirm }: ConfirmButtonProps) {
  const [armed, setArmed] = useState(false);

  useEffect(() => {
    if (!armed) {
      return;
    }
    const timer = window.setTimeout(() => setArmed(false), ARMED_FOR_MS);
    return () => window.clearTimeout(timer);
  }, [armed]);

  return (
    <button
      type="button"
      className={armed ? 'button danger' : 'button quiet'}
      disabled={disabled}
      onClick={() => {
        if (armed) {
          setArmed(false);
          onConfirm();
        } else {
          setArmed(true);
        }
      }}
    >
      {armed ? confirmLabel : label}
    </button>
  );
}
