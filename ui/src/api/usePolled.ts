import { useCallback, useEffect, useRef, useState } from 'react';

export interface Polled<T> {
  /** The latest answer; null until the first one arrives. */
  data: T | null;
  /** True while the board is not answering. */
  stale: boolean;
  /** Ask again now, for example straight after changing something. */
  refresh: () => void;
  /** Replace the data with a reply already in hand. */
  set: (value: T) => void;
}

/** Fetches something from the board now and then, for as long as the screen is shown. */
export function usePolled<T>(fetcher: () => Promise<T>, everyMs: number): Polled<T> {
  const [data, setData] = useState<T | null>(null);
  const [stale, setStale] = useState(false);
  const [nudge, setNudge] = useState(0);
  const fetcherRef = useRef(fetcher);

  useEffect(() => {
    fetcherRef.current = fetcher;
  }, [fetcher]);

  useEffect(() => {
    let disposed = false;
    let timer: number | null = null;

    const run = () => {
      fetcherRef.current().then(
        (value) => {
          if (!disposed) {
            setData(value);
            setStale(false);
            timer = window.setTimeout(run, everyMs);
          }
        },
        () => {
          if (!disposed) {
            setStale(true);
            timer = window.setTimeout(run, everyMs);
          }
        },
      );
    };
    run();

    return () => {
      disposed = true;
      if (timer !== null) {
        window.clearTimeout(timer);
      }
    };
  }, [everyMs, nudge]);

  const refresh = useCallback(() => setNudge((n) => n + 1), []);
  return { data, stale, refresh, set: setData };
}
