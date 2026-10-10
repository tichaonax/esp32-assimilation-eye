import type { ReactNode } from 'react';

export interface Fact {
  label: string;
  value: ReactNode;
}

/** A short table of names and values, such as a network's address details. */
export function Facts({ items }: { items: readonly Fact[] }) {
  return (
    <dl className="facts">
      {items.map((item) => (
        <div key={item.label}>
          <dt>{item.label}</dt>
          <dd>{item.value}</dd>
        </div>
      ))}
    </dl>
  );
}
