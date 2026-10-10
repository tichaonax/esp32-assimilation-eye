import type { EyeState } from './types';

// The board answers in well under a second. If a connection gets no answer it
// is quicker and more reliable to give up and open a new one than to wait:
// the ESP32 occasionally ignores a connection that reuses the port of one it
// has just closed, and a fresh connection gets through.
const ATTEMPT_TIMEOUT_MS = 3000;
const ATTEMPTS = 3;

export class ApiError extends Error {
  readonly status: number;

  constructor(status: number, message: string) {
    super(message);
    this.name = 'ApiError';
    this.status = status;
  }
}

async function errorMessage(response: Response): Promise<string> {
  try {
    const body: unknown = await response.json();
    if (typeof body === 'object' && body !== null && 'error' in body && typeof body.error === 'string') {
      return body.error;
    }
  } catch {
    // Not JSON; fall through to the status text.
  }
  return response.statusText || `HTTP ${response.status}`;
}

/**
 * Calls the board and returns the JSON reply. Retries when no reply arrives;
 * never retries a reply the board actually sent, whatever its status.
 */
export async function request<T>(path: string, init?: RequestInit): Promise<T> {
  let lastError: unknown;
  for (let attempt = 0; attempt < ATTEMPTS; attempt++) {
    let response: Response;
    try {
      response = await fetch(path, { ...init, signal: AbortSignal.timeout(ATTEMPT_TIMEOUT_MS) });
    } catch (error) {
      lastError = error;
      continue;
    }
    if (!response.ok) {
      throw new ApiError(response.status, await errorMessage(response));
    }
    return (await response.json()) as T;
  }
  throw lastError instanceof Error ? lastError : new Error('The device did not answer');
}

export function getState(): Promise<EyeState> {
  return request<EyeState>('/api/state');
}
