import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

import { getState } from './client';
import { applyPatch, mergePatches } from './merge';
import type { ClientMessage, EyeState, ServerMessage, SettingsPatch } from './types';

export type LinkStatus = 'connecting' | 'live' | 'reconnecting';

export interface Eye {
  /** What to show: the board's state with the user's latest edits on top. */
  state: EyeState | null;
  status: LinkStatus;
  /** The last problem the board reported, until dismissed. */
  error: string | null;
  set: (patch: SettingsPatch) => void;
  reset: () => void;
  dismissError: () => void;
}

// A dragged slider fires far more often than the LEDs need.
const SEND_INTERVAL_MS = 40;
// How long the user's own edit is shown in preference to what the board
// reports. Covers the round trip, so a slider does not jump back under the
// finger, and then lets the board's value (possibly clamped) show through.
const LOCAL_EDIT_HOLD_MS = 700;
// See client.ts: giving up on a silent connection and opening another is the
// reliable way through.
const OPEN_TIMEOUT_MS = 3000;
const RETRY_MIN_MS = 500;
const RETRY_MAX_MS = 5000;
// With nothing changing, ask for the state now and then to prove the link is
// alive; a phone that wandered off the Wi-Fi leaves a socket that looks open.
const CHECK_INTERVAL_MS = 5000;
const SILENCE_LIMIT_MS = 12000;

function socketUrl(): string {
  const scheme = window.location.protocol === 'https:' ? 'wss' : 'ws';
  return `${scheme}://${window.location.host}/ws`;
}

function parseMessage(data: unknown): ServerMessage | null {
  if (typeof data !== 'string') {
    return null;
  }
  try {
    const message: unknown = JSON.parse(data);
    if (typeof message === 'object' && message !== null && 'type' in message) {
      if (message.type === 'state' && 'state' in message) {
        return message as ServerMessage;
      }
      if (message.type === 'error' && 'error' in message && typeof message.error === 'string') {
        return message as ServerMessage;
      }
    }
  } catch {
    // Not JSON: ignore it.
  }
  return null;
}

/**
 * Live link to the board: holds one WebSocket open, reconnects by itself,
 * and sends settings as they are edited.
 */
export function useEye(): Eye {
  const [server, setServer] = useState<EyeState | null>(null);
  const [local, setLocal] = useState<SettingsPatch | null>(null);
  const [status, setStatus] = useState<LinkStatus>('connecting');
  const [error, setError] = useState<string | null>(null);

  const socketRef = useRef<WebSocket | null>(null);
  const pendingRef = useRef<SettingsPatch | null>(null);
  const sendTimerRef = useRef<number | null>(null);
  const lastSendAtRef = useRef(0);
  const localTimerRef = useRef<number | null>(null);

  const send = useCallback((message: ClientMessage): boolean => {
    const socket = socketRef.current;
    if (socket === null || socket.readyState !== WebSocket.OPEN) {
      return false;
    }
    socket.send(JSON.stringify(message));
    return true;
  }, []);

  const flush = useCallback(() => {
    sendTimerRef.current = null;
    const values = pendingRef.current;
    pendingRef.current = null;
    if (values !== null) {
      lastSendAtRef.current = performance.now();
      // If the link is down the edit is dropped; the controls are disabled
      // while it is down, and the board's state is shown on reconnect.
      send({ type: 'set', values });
    }
  }, [send]);

  const holdLocalEdit = useCallback(() => {
    if (localTimerRef.current !== null) {
      window.clearTimeout(localTimerRef.current);
    }
    localTimerRef.current = window.setTimeout(() => {
      localTimerRef.current = null;
      setLocal(null);
    }, LOCAL_EDIT_HOLD_MS);
  }, []);

  const set = useCallback(
    (patch: SettingsPatch) => {
      setLocal((current) => mergePatches(current, patch));
      holdLocalEdit();

      pendingRef.current = mergePatches(pendingRef.current, patch);
      if (sendTimerRef.current === null) {
        const wait = Math.max(0, SEND_INTERVAL_MS - (performance.now() - lastSendAtRef.current));
        sendTimerRef.current = window.setTimeout(flush, wait);
      }
    },
    [flush, holdLocalEdit],
  );

  const reset = useCallback(() => {
    pendingRef.current = null;
    setLocal(null);
    send({ type: 'reset' });
  }, [send]);

  const dismissError = useCallback(() => setError(null), []);

  useEffect(() => {
    let disposed = false;
    let retryMs = RETRY_MIN_MS;
    let retryTimer: number | null = null;
    let openTimer: number | null = null;
    let lastMessageAt = performance.now();
    let everLive = false;

    const connect = () => {
      if (disposed) {
        return;
      }
      retryTimer = null;
      const socket = new WebSocket(socketUrl());
      socketRef.current = socket;

      openTimer = window.setTimeout(() => socket.close(), OPEN_TIMEOUT_MS);

      socket.onopen = () => {
        if (openTimer !== null) {
          window.clearTimeout(openTimer);
          openTimer = null;
        }
        retryMs = RETRY_MIN_MS;
        lastMessageAt = performance.now();
        everLive = true;
        setStatus('live');
      };

      socket.onmessage = (event: MessageEvent<unknown>) => {
        lastMessageAt = performance.now();
        const message = parseMessage(event.data);
        if (message === null) {
          return;
        }
        if (message.type === 'state') {
          setServer(message.state);
        } else {
          // The board refused an edit: show why, and show what it really has.
          setError(message.error);
          setLocal(null);
        }
      };

      socket.onclose = () => {
        if (openTimer !== null) {
          window.clearTimeout(openTimer);
          openTimer = null;
        }
        if (disposed || socketRef.current !== socket) {
          return;
        }
        socketRef.current = null;
        setStatus(everLive ? 'reconnecting' : 'connecting');
        retryTimer = window.setTimeout(connect, retryMs);
        retryMs = Math.min(retryMs * 2, RETRY_MAX_MS);
      };
    };

    const reconnectNow = () => {
      if (retryTimer !== null) {
        window.clearTimeout(retryTimer);
        retryTimer = null;
      }
      retryMs = RETRY_MIN_MS;
      connect();
    };

    const check = window.setInterval(() => {
      const socket = socketRef.current;
      if (socket === null || socket.readyState !== WebSocket.OPEN) {
        return;
      }
      const silentFor = performance.now() - lastMessageAt;
      if (silentFor > SILENCE_LIMIT_MS) {
        socket.close();
      } else if (silentFor >= CHECK_INTERVAL_MS) {
        socket.send(JSON.stringify({ type: 'get' } satisfies ClientMessage));
      }
    }, CHECK_INTERVAL_MS);

    // A phone suspends the page when the screen locks; come straight back
    // when it is looked at again instead of waiting out the retry delay.
    const onVisible = () => {
      if (document.visibilityState === 'visible' && socketRef.current === null) {
        reconnectNow();
      }
    };
    document.addEventListener('visibilitychange', onVisible);

    // Fetch the state over plain HTTP too, so the page fills in even if the
    // first WebSocket attempt has to be retried.
    getState()
      .then((state) => {
        if (!disposed) {
          setServer((current) => current ?? state);
        }
      })
      .catch(() => {
        // The WebSocket will deliver it.
      });

    connect();

    return () => {
      disposed = true;
      window.clearInterval(check);
      document.removeEventListener('visibilitychange', onVisible);
      if (retryTimer !== null) {
        window.clearTimeout(retryTimer);
      }
      if (openTimer !== null) {
        window.clearTimeout(openTimer);
      }
      if (sendTimerRef.current !== null) {
        window.clearTimeout(sendTimerRef.current);
        sendTimerRef.current = null;
      }
      if (localTimerRef.current !== null) {
        window.clearTimeout(localTimerRef.current);
        localTimerRef.current = null;
      }
      const socket = socketRef.current;
      socketRef.current = null;
      socket?.close();
    };
  }, []);

  const state = useMemo(() => (server === null ? null : applyPatch(server, local)), [server, local]);

  return { state, status, error, set, reset, dismissError };
}
