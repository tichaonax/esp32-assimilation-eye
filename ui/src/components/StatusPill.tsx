import type { LinkStatus } from '../api/useEye';

const TEXT: Record<LinkStatus, string> = {
  connecting: 'Connecting…',
  live: 'Live',
  reconnecting: 'Reconnecting…',
};

/** Whether the page is in touch with the board. */
export function StatusPill({ status }: { status: LinkStatus }) {
  return (
    <span className={`pill ${status}`} role="status">
      <span className="dot" aria-hidden="true" />
      {TEXT[status]}
    </span>
  );
}
