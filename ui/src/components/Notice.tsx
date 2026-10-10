import type { ReactNode } from 'react';

export interface NoticeProps {
  kind: 'info' | 'success' | 'warning' | 'error';
  children: ReactNode;
}

/** A message inside a card: the result of an action, or something to be aware of. */
export function Notice({ kind, children }: NoticeProps) {
  return (
    <p className={`notice ${kind}`} role={kind === 'error' ? 'alert' : 'status'}>
      {children}
    </p>
  );
}
