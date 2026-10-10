import { useState } from 'react';
import type { FormEvent } from 'react';

import {
  changeCredentials,
  describeError,
  factoryReset,
  getSystem,
  logout,
  restart,
  uploadFirmware,
} from '../api/client';
import type { Session, SystemInfo } from '../api/types';
import { usePolled } from '../api/usePolled';
import { Card } from '../components/Card';
import { ConfirmButton } from '../components/ConfirmButton';
import { Facts } from '../components/Facts';
import { Notice } from '../components/Notice';
import { TextField } from '../components/TextField';

type SignedIn = Extract<Session, { authenticated: true }>;

export interface SystemScreenProps {
  session: SignedIn;
  onSessionChanged: (session: Session) => void;
  /** The board is about to restart; show the sign-in form with this explanation. */
  onRestarting: (notice: string) => void;
}

function megabytes(bytes: number): string {
  return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
}

function kilobytes(bytes: number): string {
  return `${Math.round(bytes / 1024)} KB`;
}

function uptime(seconds: number): string {
  const days = Math.floor(seconds / 86400);
  const hours = Math.floor((seconds % 86400) / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  if (days > 0) {
    return `${days} d ${hours} h`;
  }
  if (hours > 0) {
    return `${hours} h ${minutes} min`;
  }
  if (minutes > 0) {
    return `${minutes} min`;
  }
  return `${seconds} s`;
}

// ---- About --------------------------------------------------------------------------

function AboutCard({ info }: { info: SystemInfo }) {
  return (
    <Card title="This eye">
      <Facts
        items={[
          { label: 'Firmware', value: info.firmware.version },
          { label: 'Built', value: info.firmware.built },
          { label: 'LED defaults', value: info.firmware.customLedDefaults ? 'Set for this build' : 'Built-in' },
          { label: 'Running for', value: uptime(info.uptimeS) },
          { label: 'Last start', value: info.resetReason },
          { label: 'Chip', value: `${info.chip.model} rev ${info.chip.revision}` },
          { label: 'Free memory', value: kilobytes(info.memory.freeBytes) },
          {
            label: 'Firmware size',
            value: `${megabytes(info.flash.firmwareBytes)} of ${megabytes(info.flash.slotBytes)}`,
          },
        ]}
      />
      {info.firmware.developmentBuild && (
        <Notice kind="warning">
          Development build: it has a Wi-Fi name and password built in. Do not pass this firmware file on.
        </Notice>
      )}
    </Card>
  );
}

// ---- Firmware update ------------------------------------------------------------------

type UploadStage =
  | { stage: 'idle' }
  | { stage: 'sending'; fraction: number }
  | { stage: 'checking' }
  | { stage: 'failed'; message: string };

function UpdateCard({ info, onRestarting }: { info: SystemInfo; onRestarting: (notice: string) => void }) {
  const [file, setFile] = useState<File | null>(null);
  const [upload, setUpload] = useState<UploadStage>({ stage: 'idle' });

  const busy = upload.stage === 'sending' || upload.stage === 'checking';
  const tooBig = file !== null && file.size > info.flash.slotBytes;

  const start = () => {
    if (file === null || busy || tooBig) {
      return;
    }
    setUpload({ stage: 'sending', fraction: 0 });
    uploadFirmware(file, (fraction) =>
      setUpload(fraction >= 1 ? { stage: 'checking' } : { stage: 'sending', fraction }),
    ).then(
      () => onRestarting('The new firmware is installed and the eye is restarting. Sign in again in a few seconds.'),
      (problem: unknown) =>
        setUpload({
          stage: 'failed',
          message:
            problem instanceof Error && problem.name === 'ApiError'
              ? problem.message
              : 'The upload was interrupted. The eye is still running its previous firmware; try again.',
        }),
    );
  };

  return (
    <Card title="Update firmware">
      <p className="hint">
        Choose the <strong>firmware.bin</strong> produced by the build. The LEDs pulse slowly while it is installed, and
        the eye then restarts. Settings, sign-in details and the saved network are kept.
      </p>
      <div className="field">
        <input
          className="file"
          type="file"
          accept=".bin,application/octet-stream"
          aria-label="Firmware file"
          disabled={busy}
          onChange={(event) => {
            setFile(event.target.files?.[0] ?? null);
            setUpload({ stage: 'idle' });
          }}
        />
        {file !== null && (
          <p className={tooBig ? 'hint error-text' : 'hint'}>
            {file.name}, {megabytes(file.size)}
            {tooBig ? `. Too large: the limit is ${megabytes(info.flash.slotBytes)}.` : ''}
          </p>
        )}
      </div>

      {upload.stage === 'sending' && (
        <div className="field">
          <progress max={1} value={upload.fraction} aria-label="Upload progress" />
          <p className="hint">Sending… {Math.round(upload.fraction * 100)}%. Keep this page open.</p>
        </div>
      )}
      {upload.stage === 'checking' && <Notice kind="info">Sent. The eye is checking the firmware…</Notice>}
      {upload.stage === 'failed' && <Notice kind="error">{upload.message}</Notice>}

      <div className="form-actions">
        <ConfirmButton
          label="Install this firmware"
          confirmLabel="Tap again to install and restart"
          disabled={file === null || busy || tooBig}
          onConfirm={start}
        />
      </div>
    </Card>
  );
}

// ---- Sign-in details ------------------------------------------------------------------

function CredentialsCard({ session, onSessionChanged }: { session: SignedIn; onSessionChanged: (s: Session) => void }) {
  const [current, setCurrent] = useState('');
  const [username, setUsername] = useState(session.username);
  const [password, setPassword] = useState('');
  const [repeat, setRepeat] = useState('');
  const [showProblems, setShowProblems] = useState(false);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [saved, setSaved] = useState(false);

  const problems = {
    current: current === '' ? 'Needed to confirm it is you.' : null,
    username: username.length < 3 ? 'At least 3 characters.' : null,
    password: password.length < 8 ? 'At least 8 characters.' : null,
    repeat: repeat !== password ? 'The two passwords are not the same.' : null,
  };
  const anyProblem = Object.values(problems).some((problem) => problem !== null);
  const shown = (problem: string | null) => (showProblems ? problem : null);

  const submit = (event: FormEvent) => {
    event.preventDefault();
    setShowProblems(true);
    setSaved(false);
    if (anyProblem || busy) {
      return;
    }
    setBusy(true);
    setError(null);
    changeCredentials(current, username, password)
      .then((next) => {
        onSessionChanged(next);
        setCurrent('');
        setPassword('');
        setRepeat('');
        setShowProblems(false);
        setSaved(true);
      })
      .catch((problem: unknown) => setError(describeError(problem)))
      .finally(() => setBusy(false));
  };

  return (
    <Card title="Sign-in details">
      <form onSubmit={submit}>
        {session.defaultPassword && (
          <Notice kind="warning">The password is still the factory default. Change it before a demo.</Notice>
        )}
        <TextField
          label="Current password"
          type="password"
          value={current}
          onChange={setCurrent}
          autoComplete="current-password"
          maxLength={64}
          error={shown(problems.current)}
        />
        <TextField
          label="User name"
          value={username}
          onChange={setUsername}
          autoComplete="username"
          maxLength={32}
          error={shown(problems.username)}
          hint="3 to 32 characters."
        />
        <TextField
          label="New password"
          type="password"
          value={password}
          onChange={setPassword}
          autoComplete="new-password"
          maxLength={64}
          error={shown(problems.password)}
          hint="8 to 64 characters."
        />
        <TextField
          label="New password again"
          type="password"
          value={repeat}
          onChange={setRepeat}
          autoComplete="new-password"
          maxLength={64}
          error={shown(problems.repeat)}
        />
        {error !== null && <Notice kind="error">{error}</Notice>}
        {saved && (
          <Notice kind="success">
            Saved. Every other browser that was signed in has been signed out. If these details are ever lost, a factory
            reset restores the defaults.
          </Notice>
        )}
        <div className="form-actions">
          <button type="submit" className="button primary" disabled={busy}>
            {busy ? 'Saving…' : 'Save sign-in details'}
          </button>
        </div>
      </form>
    </Card>
  );
}

// ---- Restart and reset ------------------------------------------------------------------

function ResetCard({ onRestarting }: { onRestarting: (notice: string) => void }) {
  const [password, setPassword] = useState('');
  const [error, setError] = useState<string | null>(null);

  return (
    <Card title="Restart and reset">
      {error !== null && <Notice kind="error">{error}</Notice>}
      <div className="form-actions start">
        <ConfirmButton
          label="Restart the eye"
          confirmLabel="Tap again to restart"
          onConfirm={() => {
            setError(null);
            restart().then(
              () => onRestarting('The eye is restarting. Sign in again in a few seconds.'),
              (problem: unknown) => setError(describeError(problem)),
            );
          }}
        />
      </div>

      <p className="hint spaced">
        A factory reset restores the default sign-in, forgets the saved network, restores the access point&apos;s name
        and password, and resets the LED settings. Holding the BOOT button on the board for ten seconds does the same.
      </p>
      <TextField
        label="Current password"
        type="password"
        value={password}
        onChange={setPassword}
        autoComplete="current-password"
        maxLength={64}
      />
      <div className="form-actions start">
        <ConfirmButton
          label="Factory reset"
          confirmLabel="Tap again: everything will be erased"
          disabled={password === ''}
          onConfirm={() => {
            setError(null);
            factoryReset(password).then(
              () =>
                onRestarting(
                  'Factory reset done and the eye is restarting. It may now only be on its own access point, with the default sign-in.',
                ),
              (problem: unknown) => setError(describeError(problem)),
            );
          }}
        />
      </div>
    </Card>
  );
}

// ---- The screen ---------------------------------------------------------------------------

export function SystemScreen({ session, onSessionChanged, onRestarting }: SystemScreenProps) {
  const system = usePolled(getSystem, 5000);
  const [signOutError, setSignOutError] = useState<string | null>(null);

  return (
    <>
      {system.data === null ? (
        <p className="placeholder">{system.stale ? 'The eye is not answering…' : 'Reading the system details…'}</p>
      ) : (
        <>
          <AboutCard info={system.data} />
          <UpdateCard info={system.data} onRestarting={onRestarting} />
        </>
      )}
      <CredentialsCard session={session} onSessionChanged={onSessionChanged} />
      <Card title="Signed in">
        <p className="hint">
          Signed in as <strong>{session.username}</strong>.
        </p>
        {signOutError !== null && <Notice kind="error">{signOutError}</Notice>}
        <div className="form-actions start">
          <button
            type="button"
            className="button"
            onClick={() => {
              setSignOutError(null);
              logout().then(onSessionChanged, (problem: unknown) => setSignOutError(describeError(problem)));
            }}
          >
            Sign out
          </button>
        </div>
      </Card>
      <ResetCard onRestarting={onRestarting} />
    </>
  );
}
