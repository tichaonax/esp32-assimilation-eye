import { useEffect, useRef, useState } from 'react';
import type { FormEvent } from 'react';

import { describeError, forgetWifi, getWifi, joinWifi, scanWifi, setAccessPoint } from '../api/client';
import type { Addressing, JoinRequest, NetworkFound, StaticAddress, StationReport, WifiReport } from '../api/types';
import { usePolled } from '../api/usePolled';
import { Card } from '../components/Card';
import { ConfirmButton } from '../components/ConfirmButton';
import { Facts } from '../components/Facts';
import type { Fact } from '../components/Facts';
import { Notice } from '../components/Notice';
import { Segmented } from '../components/Segmented';
import type { SegmentedOption } from '../components/Segmented';
import { TextField } from '../components/TextField';

const ADDRESSING: readonly SegmentedOption<Addressing>[] = [
  { value: 'dhcp', label: 'Automatic (DHCP)' },
  { value: 'static', label: 'Static' },
];

const IPV4 = /^(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}$/;

/** A message if the text is present and is not an address; the board checks the rest. */
function addressProblem(text: string, required: boolean): string | null {
  if (text === '') {
    return required ? 'Required for a static address.' : null;
  }
  return IPV4.test(text) ? null : 'Not a valid address. Example: 192.168.1.50';
}

function signalWords(rssi: number): string {
  if (rssi >= -60) {
    return 'strong';
  }
  if (rssi >= -72) {
    return 'good';
  }
  if (rssi >= -82) {
    return 'weak';
  }
  return 'very weak';
}

function statusLine(station: StationReport): { kind: 'success' | 'info' | 'warning' | 'error'; text: string } {
  switch (station.status) {
    case 'connected':
      return { kind: 'success', text: `Joined to ${station.ssid}.` };
    case 'connecting':
      return {
        kind: 'info',
        text: `Joining ${station.ssid}…${station.problem ? ` Last attempt: ${station.problem}.` : ''}`,
      };
    case 'failed':
      return {
        kind: 'error',
        text: `Could not join ${station.ssid}: ${station.problem || 'no answer'}. The eye keeps trying every half minute.`,
      };
    case 'notConfigured':
      return { kind: 'info', text: 'Not joined to any network. The eye is reachable on its own access point only.' };
  }
}

// ---- The joined network -----------------------------------------------------------

function StationCard({ wifi, onChanged }: { wifi: WifiReport; onChanged: (report: WifiReport) => void }) {
  const { station } = wifi;
  const [error, setError] = useState<string | null>(null);
  const line = statusLine(station);

  const facts: Fact[] = [];
  if (station.status === 'connected') {
    facts.push(
      { label: 'Address', value: station.ip },
      { label: 'Addressing', value: station.addressing === 'dhcp' ? 'Automatic (DHCP)' : 'Static' },
      { label: 'Subnet mask', value: station.mask },
      { label: 'Gateway', value: station.gateway },
      { label: 'DNS', value: [station.dns1, station.dns2].filter(Boolean).join(', ') || '—' },
    );
    if (station.rssi !== undefined) {
      facts.push({ label: 'Signal', value: `${signalWords(station.rssi)} (${station.rssi} dBm)` });
    }
  } else if (station.configured) {
    facts.push({ label: 'Addressing', value: station.addressing === 'dhcp' ? 'Automatic (DHCP)' : `Static, ${station.static.ip}` });
  }

  return (
    <Card title="Joined network">
      <Notice kind={line.kind}>{line.text}</Notice>
      {facts.length > 0 && <Facts items={facts} />}
      {station.status === 'connected' && (
        <p className="hint">
          From a device on that network, open the eye at <strong>http://{station.ip}</strong> or{' '}
          <strong>http://{wifi.hostname}.local</strong>. The same sign-in applies.
        </p>
      )}
      {station.fromBuild && (
        <p className="hint">This network was built into a development firmware. Joining one below replaces it.</p>
      )}
      {error !== null && <Notice kind="error">{error}</Notice>}
      {station.configured && (
        <div className="form-actions">
          <ConfirmButton
            label="Forget this network"
            confirmLabel="Tap again to forget it"
            onConfirm={() => {
              setError(null);
              forgetWifi().then(onChanged, (problem: unknown) => setError(describeError(problem)));
            }}
          />
        </div>
      )}
    </Card>
  );
}

// ---- Joining a network --------------------------------------------------------------

function JoinCard({ wifi, onChanged }: { wifi: WifiReport; onChanged: (report: WifiReport) => void }) {
  const { station } = wifi;
  const [ssid, setSsid] = useState(station.configured ? station.ssid : '');
  const [password, setPassword] = useState('');
  const [addressing, setAddressing] = useState<Addressing>(station.addressing);
  const [fixed, setFixed] = useState<StaticAddress>(station.static);
  const [showProblems, setShowProblems] = useState(false);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [sent, setSent] = useState(false);

  const [networks, setNetworks] = useState<NetworkFound[] | null>(null);
  const [searching, setSearching] = useState(false);
  const searchTimer = useRef<number | null>(null);

  useEffect(
    () => () => {
      if (searchTimer.current !== null) {
        window.clearTimeout(searchTimer.current);
      }
    },
    [],
  );

  const search = () => {
    setSearching(true);
    setError(null);
    const poll = (startNew: boolean) => {
      scanWifi(startNew).then(
        (report) => {
          if (report.scanning) {
            searchTimer.current = window.setTimeout(() => poll(false), 1000);
          } else {
            setNetworks(report.networks);
            setSearching(false);
          }
        },
        (problem: unknown) => {
          setError(describeError(problem));
          setSearching(false);
        },
      );
    };
    poll(true);
  };

  const sameNetwork = station.configured && ssid === station.ssid;
  const problems = {
    ip: addressing === 'static' ? addressProblem(fixed.ip, true) : null,
    mask: addressing === 'static' ? addressProblem(fixed.mask, true) : null,
    gateway: addressing === 'static' ? addressProblem(fixed.gateway, true) : null,
    dns1: addressing === 'static' ? addressProblem(fixed.dns1, false) : null,
    dns2: addressing === 'static' ? addressProblem(fixed.dns2, false) : null,
  };
  const anyProblem = Object.values(problems).some((problem) => problem !== null);
  const shown = (problem: string | null) => (showProblems ? problem : null);

  const submit = (event: FormEvent) => {
    event.preventDefault();
    setShowProblems(true);
    setSent(false);
    if (ssid === '' || anyProblem || busy) {
      return;
    }
    const request: JoinRequest = { ssid, addressing };
    // An empty password for the network already saved means "keep it".
    if (!(sameNetwork && password === '')) {
      request.password = password;
    }
    if (addressing === 'static') {
      Object.assign(request, fixed);
    }
    setBusy(true);
    setError(null);
    joinWifi(request)
      .then((report) => {
        onChanged(report);
        setPassword('');
        setSent(true);
        setShowProblems(false);
      })
      .catch((problem: unknown) => setError(describeError(problem)))
      .finally(() => setBusy(false));
  };

  const field = (key: keyof StaticAddress) => (value: string) => setFixed((current) => ({ ...current, [key]: value.trim() }));

  return (
    <Card title="Join a network">
      <form onSubmit={submit}>
        <div className="form-actions start">
          <button type="button" className="button" onClick={search} disabled={searching}>
            {searching ? 'Searching…' : networks === null ? 'Search for networks' : 'Search again'}
          </button>
        </div>
        {networks !== null && networks.length === 0 && (
          <p className="hint">No networks found. The eye only sees 2.4 GHz networks.</p>
        )}
        {networks !== null && networks.length > 0 && (
          <ul className="networks">
            {networks.map((network) => (
              <li key={network.ssid}>
                <button
                  type="button"
                  className={network.ssid === ssid ? 'network chosen' : 'network'}
                  onClick={() => {
                    setSsid(network.ssid);
                    setPassword('');
                    setSent(false);
                  }}
                >
                  <span className="network-name">{network.ssid}</span>
                  <span className="network-detail">
                    {network.secure ? 'password' : 'open'} · {signalWords(network.rssi)}
                  </span>
                </button>
              </li>
            ))}
          </ul>
        )}

        <TextField
          label="Network name"
          value={ssid}
          onChange={(value) => {
            setSsid(value);
            setSent(false);
          }}
          maxLength={32}
          error={showProblems && ssid === '' ? 'Choose a network above or type its name.' : null}
          hint="Exactly as the network shows it: capitals and spaces matter."
        />
        <TextField
          label="Password"
          type="password"
          value={password}
          onChange={setPassword}
          autoComplete="off"
          maxLength={63}
          hint={
            sameNetwork
              ? 'Leave empty to keep the password already saved for this network.'
              : 'Leave empty for an open network.'
          }
        />
        <Segmented label="Address" value={addressing} options={ADDRESSING} onChange={setAddressing} />
        {addressing === 'dhcp' ? (
          <p className="hint">The network gives the eye an address. It is shown above once joined.</p>
        ) : (
          <>
            <TextField
              label="IP address"
              value={fixed.ip}
              onChange={field('ip')}
              inputMode="decimal"
              placeholder="192.168.1.50"
              error={shown(problems.ip)}
              hint="Pick one outside the range your router hands out automatically."
            />
            <TextField
              label="Subnet mask"
              value={fixed.mask}
              onChange={field('mask')}
              inputMode="decimal"
              placeholder="255.255.255.0"
              error={shown(problems.mask)}
            />
            <TextField
              label="Gateway"
              value={fixed.gateway}
              onChange={field('gateway')}
              inputMode="decimal"
              placeholder="192.168.1.1"
              error={shown(problems.gateway)}
              hint="Usually your router's address."
            />
            <TextField
              label="DNS server (optional)"
              value={fixed.dns1}
              onChange={field('dns1')}
              inputMode="decimal"
              error={shown(problems.dns1)}
              hint="Leave empty to use the gateway."
            />
            <TextField
              label="Second DNS server (optional)"
              value={fixed.dns2}
              onChange={field('dns2')}
              inputMode="decimal"
              error={shown(problems.dns2)}
            />
            <p className="hint">
              If a static address turns out wrong, the eye stays reachable on its own access point, where this can be
              corrected.
            </p>
          </>
        )}

        {error !== null && <Notice kind="error">{error}</Notice>}
        {sent && (
          <Notice kind="info">
            Saved. The eye is joining now; the result appears above. If you are connected through the eye&apos;s own
            access point, this page may lose contact for a moment.
          </Notice>
        )}
        <div className="form-actions">
          <button type="submit" className="button primary" disabled={busy}>
            {busy ? 'Saving…' : 'Save and join'}
          </button>
        </div>
      </form>
    </Card>
  );
}

// ---- The eye's own access point -------------------------------------------------------

function AccessPointCard({ wifi, onChanged }: { wifi: WifiReport; onChanged: () => void }) {
  const { accessPoint } = wifi;
  const [ssid, setSsid] = useState(accessPoint.ssid);
  const [password, setPassword] = useState('');
  const [error, setError] = useState<string | null>(null);
  const [saved, setSaved] = useState(false);

  const tooShort = password.length < 8;

  return (
    <Card title="The eye's own access point">
      <Facts
        items={[
          { label: 'Name', value: accessPoint.ssid },
          { label: 'Address', value: accessPoint.ip },
          { label: 'Devices connected', value: accessPoint.clients },
        ]}
      />
      <p className="hint">Always on, so the eye can be reached even where there is no other network.</p>
      {accessPoint.defaultPassword && (
        <Notice kind="warning">The access point still has its factory password. Anyone who knows it can connect.</Notice>
      )}
      <TextField label="New name" value={ssid} onChange={setSsid} maxLength={32} />
      <TextField
        label="New password"
        type="password"
        value={password}
        onChange={setPassword}
        autoComplete="new-password"
        maxLength={63}
        hint="8 to 63 characters."
      />
      {error !== null && <Notice kind="error">{error}</Notice>}
      {saved && (
        <Notice kind="success">
          Saved. Devices on the access point are disconnected and must join again with the new details.
        </Notice>
      )}
      <div className="form-actions">
        <ConfirmButton
          label="Save access point"
          confirmLabel="Tap again: devices on it will be disconnected"
          disabled={ssid === '' || tooShort}
          onConfirm={() => {
            setError(null);
            setSaved(false);
            setAccessPoint(ssid, password).then(
              () => {
                setSaved(true);
                setPassword('');
                onChanged();
              },
              (problem: unknown) => setError(describeError(problem)),
            );
          }}
        />
      </div>
    </Card>
  );
}

// ---- The screen -------------------------------------------------------------------------

export function WifiScreen() {
  const wifi = usePolled(getWifi, 3000);

  if (wifi.data === null) {
    return <p className="placeholder">{wifi.stale ? 'The eye is not answering…' : 'Reading the Wi-Fi status…'}</p>;
  }
  return (
    <>
      {wifi.stale && (
        <p className="banner" role="status">
          The eye is not answering. What is shown may be out of date.
        </p>
      )}
      <StationCard wifi={wifi.data} onChanged={wifi.set} />
      <JoinCard wifi={wifi.data} onChanged={wifi.set} />
      <AccessPointCard wifi={wifi.data} onChanged={wifi.refresh} />
    </>
  );
}
