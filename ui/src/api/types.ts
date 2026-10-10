// The shapes the board sends and accepts. They mirror
// lib/eye_core/ConfigJson.h and src/WebApi.h in the firmware.

export type Direction = 'clockwise' | 'counterClockwise';

export type OnboardMode = 'off' | 'on' | 'heartbeat' | 'blinkOnPatternChange';

export interface CometSettings {
  direction: Direction;
  stepIntervalMs: number;
  headBrightness: number;
  tailLength: number;
  tailFalloff: number;
}

export interface FlashSettings {
  intervalMs: number;
  durationMs: number;
  minBrightness: number;
  maxBrightness: number;
  rampUpMs: number;
  rampDownMs: number;
}

export interface OnboardSettings {
  mode: OnboardMode;
  brightness: number;
  blinkCount: number;
  blinkMs: number;
}

export interface EyeSettings {
  running: boolean;
  pattern: string;
  brightness: number;
  comet: CometSettings;
  flash: FlashSettings;
  onboard: OnboardSettings;
}

export interface PatternInfo {
  name: string;
  label: string;
}

/** Everything GET /api/state and the WebSocket report. */
export interface EyeState extends EyeSettings {
  patterns: PatternInfo[];
  onboardAvailable: boolean;
}

/** Any subset of the settings, as accepted by POST /api/config and "set". */
export interface SettingsPatch {
  running?: boolean;
  pattern?: string;
  brightness?: number;
  comet?: Partial<CometSettings>;
  flash?: Partial<FlashSettings>;
  onboard?: Partial<OnboardSettings>;
}

export type ClientMessage = { type: 'set'; values: SettingsPatch } | { type: 'reset' } | { type: 'get' };

export type ServerMessage = { type: 'state'; state: EyeState } | { type: 'error'; error: string };

/** Ranges from the firmware's EyeConfig.h. The board clamps regardless. */
export const LIMITS = {
  percent: { min: 0, max: 100 },
  cometStepMs: { min: 20, max: 2000 },
  cometTail: { min: 0, max: 7 },
  cometFalloff: { min: 10, max: 90 },
  flashIntervalMs: { min: 100, max: 10000 },
  flashDurationMs: { min: 0, max: 10000 },
  flashRampMs: { min: 0, max: 5000 },
  onboardBlinkCount: { min: 1, max: 5 },
  onboardBlinkMs: { min: 50, max: 500 },
} as const;

// ---- Sign-in ------------------------------------------------------------------

export type Session =
  | { authenticated: false }
  | { authenticated: true; username: string; defaultPassword: boolean };

// ---- Wi-Fi --------------------------------------------------------------------

export type StationStatus = 'notConfigured' | 'connecting' | 'connected' | 'failed';
export type Addressing = 'dhcp' | 'static';

export interface StaticAddress {
  ip: string;
  mask: string;
  gateway: string;
  dns1: string;
  dns2: string;
}

export interface StationReport extends StaticAddress {
  configured: boolean;
  /** The network came from a development build, not from this screen. */
  fromBuild: boolean;
  ssid: string;
  status: StationStatus;
  /** Why joining failed, in plain words; empty if there is nothing to say. */
  problem: string;
  addressing: Addressing;
  rssi?: number;
  channel?: number;
  /** What was last typed in for static addressing. */
  static: StaticAddress;
}

export interface AccessPointReport {
  ssid: string;
  ip: string;
  clients: number;
  defaultPassword: boolean;
}

export interface WifiReport {
  station: StationReport;
  accessPoint: AccessPointReport;
  hostname: string;
  mac: string;
}

export interface NetworkFound {
  ssid: string;
  rssi: number;
  secure: boolean;
  channel: number;
}

export interface ScanReport {
  scanning: boolean;
  networks: NetworkFound[];
}

export interface JoinRequest extends Partial<StaticAddress> {
  ssid: string;
  /** Left out to keep the password already saved for this network. */
  password?: string;
  addressing: Addressing;
}

// ---- System -------------------------------------------------------------------

export interface SystemInfo {
  firmware: { version: string; built: string; developmentBuild: boolean };
  chip: { model: string; revision: string; cores: number; mac: string };
  uptimeS: number;
  resetReason: string;
  memory: { freeBytes: number; lowestFreeBytes: number };
  flash: { sizeBytes: number; firmwareBytes: number; slotBytes: number };
  animation: { frames: number; tickMs: number; worstGapMs: number };
}
