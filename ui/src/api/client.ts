import type {
  AccessPointReport,
  EyeState,
  JoinRequest,
  ScanReport,
  Session,
  SystemInfo,
  WifiReport,
} from './types';

// The board answers in well under a second. If a connection gets no answer it
// is quicker and more reliable to give up and open a new one than to wait:
// the ESP32 occasionally ignores a connection that reuses the port of one it
// has just closed, and a fresh connection gets through.
const READ_TIMEOUT_MS = 3000;
const READ_ATTEMPTS = 3;
// A change is sent once. If no reply comes it may or may not have been
// applied, so the caller is told and decides; it is given longer to answer
// because checking a password takes the board a noticeable moment.
const CHANGE_TIMEOUT_MS = 10000;
const UPLOAD_TIMEOUT_MS = 180000;

export class ApiError extends Error {
  readonly status: number;
  /** For 429: how long the board asks the caller to wait. */
  readonly retryAfterS: number | undefined;

  constructor(status: number, message: string, retryAfterS?: number) {
    super(message);
    this.name = 'ApiError';
    this.status = status;
    this.retryAfterS = retryAfterS;
  }
}

/** A message fit to show for anything a call can throw. */
export function describeError(error: unknown): string {
  if (error instanceof ApiError) {
    return error.message;
  }
  return 'The eye did not answer. Check that you are still on its network and try again.';
}

let onSignedOut: (() => void) | null = null;

/** Called when the board says the session is no longer valid. */
export function setSignedOutHandler(handler: (() => void) | null): void {
  onSignedOut = handler;
}

function parseError(status: number, statusText: string, body: unknown): ApiError {
  let message = statusText || `HTTP ${status}`;
  let retryAfterS: number | undefined;
  if (typeof body === 'object' && body !== null) {
    if ('error' in body && typeof body.error === 'string') {
      message = body.error;
    }
    if ('retryAfterS' in body && typeof body.retryAfterS === 'number') {
      retryAfterS = body.retryAfterS;
    }
  }
  return new ApiError(status, message, retryAfterS);
}

/** Asks the board whether this browser is still signed in, and says so if it is not. */
export async function confirmStillSignedIn(): Promise<void> {
  const session = await read<Session>('/api/session');
  if (!session.authenticated) {
    onSignedOut?.();
  }
}

async function readReply<T>(path: string, response: Response): Promise<T> {
  let body: unknown = null;
  try {
    body = await response.json();
  } catch {
    // Not JSON; the status decides.
  }
  if (!response.ok) {
    // A wrong password at sign-in is also 401, and is not a lost session.
    if (response.status === 401 && path !== '/api/login') {
      onSignedOut?.();
    }
    throw parseError(response.status, response.statusText, body);
  }
  return body as T;
}

/** Reads from the board. Retried when no reply arrives; a reply is never retried. */
async function read<T>(path: string): Promise<T> {
  let lastError: unknown;
  for (let attempt = 0; attempt < READ_ATTEMPTS; attempt++) {
    let response: Response;
    try {
      response = await fetch(path, { signal: AbortSignal.timeout(READ_TIMEOUT_MS) });
    } catch (error) {
      lastError = error;
      continue;
    }
    return readReply<T>(path, response);
  }
  throw lastError instanceof Error ? lastError : new Error('no answer');
}

/** Asks the board to change something. Sent once. */
async function change<T>(method: 'POST' | 'DELETE', path: string, body?: unknown): Promise<T> {
  const response = await fetch(path, {
    method,
    signal: AbortSignal.timeout(CHANGE_TIMEOUT_MS),
    ...(body === undefined ? {} : { headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }),
  });
  return readReply<T>(path, response);
}

// ---- LED state ------------------------------------------------------------------

export const getState = () => read<EyeState>('/api/state');

// ---- Sign-in --------------------------------------------------------------------

export const getSession = () => read<Session>('/api/session');

export const login = (username: string, password: string) =>
  change<Session>('POST', '/api/login', { username, password });

export const logout = () => change<Session>('POST', '/api/logout');

export const changeCredentials = (currentPassword: string, username: string, password: string) =>
  change<Session>('POST', '/api/credentials', { currentPassword, username, password });

// ---- Wi-Fi ----------------------------------------------------------------------

export const getWifi = () => read<WifiReport>('/api/wifi');

export const scanWifi = (startNew: boolean) => read<ScanReport>(startNew ? '/api/wifi/scan?start=1' : '/api/wifi/scan');

export const joinWifi = (request: JoinRequest) => change<WifiReport>('POST', '/api/wifi', request);

export const forgetWifi = () => change<WifiReport>('DELETE', '/api/wifi');

export const setAccessPoint = (ssid: string, password: string) =>
  change<{ accessPoint: AccessPointReport }>('POST', '/api/wifi/ap', { ssid, password });

// ---- System ---------------------------------------------------------------------

export const getSystem = () => read<SystemInfo>('/api/system');

export const restart = () => change<{ restarting: boolean }>('POST', '/api/restart');

export const factoryReset = (currentPassword: string) =>
  change<{ restarting: boolean }>('POST', '/api/factory-reset', { currentPassword });

/**
 * Uploads a firmware file. onProgress is given 0..1 as the file is sent; the
 * promise settles when the board has checked the whole image.
 */
export function uploadFirmware(file: File, onProgress: (fraction: number) => void): Promise<void> {
  // XMLHttpRequest, because fetch cannot report how much has been sent.
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/update');
    xhr.timeout = UPLOAD_TIMEOUT_MS;
    xhr.upload.onprogress = (event) => {
      if (event.lengthComputable) {
        onProgress(event.loaded / event.total);
      }
    };
    xhr.onload = () => {
      if (xhr.status === 200) {
        resolve();
        return;
      }
      let body: unknown = null;
      try {
        body = JSON.parse(xhr.responseText);
      } catch {
        // Not JSON; the status decides.
      }
      if (xhr.status === 401) {
        onSignedOut?.();
      }
      reject(parseError(xhr.status, xhr.statusText, body));
    };
    xhr.onerror = () => reject(new Error('upload interrupted'));
    xhr.ontimeout = () => reject(new Error('upload timed out'));

    const form = new FormData();
    form.append('firmware', file, file.name);
    xhr.send(form);
  });
}
