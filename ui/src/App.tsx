import { useCallback, useEffect, useRef, useState } from 'react';
import type { ReactNode } from 'react';

import { getSession, setSignedOutHandler } from './api/client';
import type { Session } from './api/types';
import { useEye } from './api/useEye';
import { StatusPill } from './components/StatusPill';
import { EyeScreen } from './screens/EyeScreen';
import { SignInScreen } from './screens/SignInScreen';
import { SystemScreen } from './screens/SystemScreen';
import { WifiScreen } from './screens/WifiScreen';

type SignedIn = Extract<Session, { authenticated: true }>;

type Tab = 'eye' | 'wifi' | 'system';
const TABS: readonly { id: Tab; label: string }[] = [
  { id: 'eye', label: 'Eye' },
  { id: 'wifi', label: 'Wi-Fi' },
  { id: 'system', label: 'System' },
];

function tabFromAddress(): Tab {
  const name = window.location.hash.replace('#', '');
  return TABS.some((tab) => tab.id === name) ? (name as Tab) : 'eye';
}

function Header({ children }: { children?: ReactNode }) {
  return (
    <header className="top">
      <div className="brand">
        <span className="lens" aria-hidden="true" />
        <h1>Assimilation Eye</h1>
      </div>
      {children}
    </header>
  );
}

/** Everything behind the sign-in. Mounted only while signed in, so the live link lives exactly that long. */
function SignedInApp({
  session,
  onSessionChanged,
  onRestarting,
}: {
  session: SignedIn;
  onSessionChanged: (session: Session) => void;
  onRestarting: (notice: string) => void;
}) {
  const eye = useEye();
  const [tab, setTab] = useState<Tab>(tabFromAddress);

  // The tab is kept in the address, so the back button and a reload behave.
  useEffect(() => {
    const onHashChange = () => setTab(tabFromAddress());
    window.addEventListener('hashchange', onHashChange);
    return () => window.removeEventListener('hashchange', onHashChange);
  }, []);

  return (
    <>
      <Header>
        <StatusPill status={eye.status} />
      </Header>

      <nav className="tabs" aria-label="Sections">
        {TABS.map((item) => (
          <a key={item.id} href={`#${item.id}`} aria-current={item.id === tab ? 'page' : undefined}>
            {item.label}
          </a>
        ))}
      </nav>

      {eye.status === 'reconnecting' && (
        <p className="banner" role="status">
          Lost contact with the eye. Trying again. If it still has power, the LEDs carry on as they were.
        </p>
      )}

      {session.defaultPassword && tab !== 'system' && (
        <div className="banner" role="status">
          <span>The password is still the factory default.</span>
          <a className="button quiet small" href="#system">
            Change it
          </a>
        </div>
      )}

      {eye.error !== null && (
        <div className="banner error" role="alert">
          <span>The eye refused that change: {eye.error}</span>
          <button type="button" className="button quiet small" onClick={eye.dismissError}>
            Dismiss
          </button>
        </div>
      )}

      <main>
        {tab === 'eye' && <EyeScreen eye={eye} />}
        {tab === 'wifi' && <WifiScreen />}
        {tab === 'system' && (
          <SystemScreen session={session} onSessionChanged={onSessionChanged} onRestarting={onRestarting} />
        )}
      </main>
    </>
  );
}

const RETRY_MS = 2000;

export function App() {
  // null until the board has said whether this browser is signed in.
  const [session, setSession] = useState<Session | null>(null);
  const [notice, setNotice] = useState<string | null>(null);
  const [unreachable, setUnreachable] = useState(false);

  const signedInRef = useRef(false);
  useEffect(() => {
    signedInRef.current = session?.authenticated === true;
  }, [session]);

  // Ask the board, and keep asking until it answers.
  useEffect(() => {
    if (session !== null) {
      return;
    }
    let disposed = false;
    let timer: number | null = null;
    const ask = () => {
      getSession().then(
        (answer) => {
          if (!disposed) {
            setUnreachable(false);
            setSession(answer);
          }
        },
        () => {
          if (!disposed) {
            setUnreachable(true);
            timer = window.setTimeout(ask, RETRY_MS);
          }
        },
      );
    };
    ask();
    return () => {
      disposed = true;
      if (timer !== null) {
        window.clearTimeout(timer);
      }
    };
  }, [session]);

  // Any call that finds the session gone brings the sign-in form back.
  useEffect(() => {
    setSignedOutHandler(() => {
      // Only news if this page thought it was signed in. After a deliberate
      // sign-out, or during a restart, there is nothing to explain.
      if (signedInRef.current) {
        setSession({ authenticated: false });
        setNotice('You have been signed out. Sign in again to continue.');
      }
    });
    return () => setSignedOutHandler(null);
  }, []);

  const onSessionChanged = useCallback((next: Session) => {
    setNotice(null);
    setSession(next);
  }, []);

  // A restart ends every session. Show the form, with the reason, and let the
  // loop above wait for the board to come back.
  const onRestarting = useCallback((reason: string) => {
    setNotice(reason);
    setSession(null);
  }, []);

  return (
    <div className="app">
      {session === null && (
        <>
          <Header />
          {notice !== null && <p className="banner">{notice}</p>}
          <p className="placeholder">{unreachable ? 'Waiting for the eye to answer…' : 'Contacting the eye…'}</p>
        </>
      )}
      {session !== null && !session.authenticated && (
        <>
          <Header />
          <main>
            <SignInScreen notice={notice} onSignedIn={onSessionChanged} />
          </main>
        </>
      )}
      {session !== null && session.authenticated && (
        <SignedInApp session={session} onSessionChanged={onSessionChanged} onRestarting={onRestarting} />
      )}
    </div>
  );
}
