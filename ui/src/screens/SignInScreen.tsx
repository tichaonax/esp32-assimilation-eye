import { useEffect, useState } from 'react';
import type { FormEvent } from 'react';

import { ApiError, describeError, login } from '../api/client';
import type { Session } from '../api/types';
import { Card } from '../components/Card';
import { Notice } from '../components/Notice';
import { TextField } from '../components/TextField';

export interface SignInScreenProps {
  /** Why the form is being shown again, e.g. after a restart. */
  notice: string | null;
  onSignedIn: (session: Session) => void;
}

export function SignInScreen({ notice, onSignedIn }: SignInScreenProps) {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [lockedForS, setLockedForS] = useState(0);

  // Count the lockout down, so it is clear when to try again.
  useEffect(() => {
    if (lockedForS <= 0) {
      return;
    }
    const timer = window.setTimeout(() => setLockedForS((s) => s - 1), 1000);
    return () => window.clearTimeout(timer);
  }, [lockedForS]);

  const submit = (event: FormEvent) => {
    event.preventDefault();
    if (busy || lockedForS > 0) {
      return;
    }
    setBusy(true);
    setError(null);
    login(username, password)
      .then(onSignedIn)
      .catch((problem: unknown) => {
        if (problem instanceof ApiError && problem.status === 429) {
          setLockedForS(problem.retryAfterS ?? 30);
          setError(null);
        } else {
          setError(describeError(problem));
        }
        setPassword('');
      })
      .finally(() => setBusy(false));
  };

  return (
    <Card title="Sign in">
      <form onSubmit={submit}>
        {notice !== null && <Notice kind="info">{notice}</Notice>}
        <TextField
          label="User name"
          value={username}
          onChange={setUsername}
          autoComplete="username"
          maxLength={32}
          disabled={busy}
        />
        <TextField
          label="Password"
          type="password"
          value={password}
          onChange={setPassword}
          autoComplete="current-password"
          maxLength={64}
          disabled={busy}
        />
        {error !== null && <Notice kind="error">{error}</Notice>}
        {lockedForS > 0 && (
          <Notice kind="warning">
            Too many wrong attempts. Sign-in is locked for {lockedForS} more {lockedForS === 1 ? 'second' : 'seconds'}.
          </Notice>
        )}
        <div className="form-actions">
          <button
            type="submit"
            className="button primary"
            disabled={busy || lockedForS > 0 || username === '' || password === ''}
          >
            {busy ? 'Signing in…' : 'Sign in'}
          </button>
        </div>
      </form>
    </Card>
  );
}
