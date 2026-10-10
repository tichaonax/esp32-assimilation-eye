import type { ReactNode } from 'react';

export interface CardProps {
  title: string;
  /** Shown beside the title, e.g. a status word. */
  aside?: ReactNode;
  children: ReactNode;
}

export function Card({ title, aside, children }: CardProps) {
  return (
    <section className="card">
      <header className="card-head">
        <h2>{title}</h2>
        {aside}
      </header>
      {children}
    </section>
  );
}
