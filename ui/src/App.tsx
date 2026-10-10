import { useEye } from './api/useEye';
import { StatusPill } from './components/StatusPill';
import { EyeScreen } from './screens/EyeScreen';

export function App() {
  const eye = useEye();

  return (
    <div className="app">
      <header className="top">
        <div className="brand">
          <span className="lens" aria-hidden="true" />
          <h1>Assimilation Eye</h1>
        </div>
        <StatusPill status={eye.status} />
      </header>

      {eye.status === 'reconnecting' && (
        <p className="banner" role="status">
          Lost contact with the eye. Trying again. If it still has power, the LEDs carry on as they were.
        </p>
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
        <EyeScreen eye={eye} />
      </main>
    </div>
  );
}
