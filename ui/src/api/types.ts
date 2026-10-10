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
