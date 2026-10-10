import type { Eye } from '../api/useEye';
import { LIMITS } from '../api/types';
import type { Direction, OnboardMode } from '../api/types';
import { Card } from '../components/Card';
import { ConfirmButton } from '../components/ConfirmButton';
import { Segmented } from '../components/Segmented';
import type { SegmentedOption } from '../components/Segmented';
import { Slider } from '../components/Slider';

const DIRECTIONS: readonly SegmentedOption<Direction>[] = [
  { value: 'counterClockwise', label: 'Counter-clockwise' },
  { value: 'clockwise', label: 'Clockwise' },
];

const RANDOM_DIRECTION: readonly SegmentedOption<'on' | 'off'>[] = [
  { value: 'on', label: 'Random' },
  { value: 'off', label: 'As set for the comet' },
];

const ONBOARD_MODES: readonly SegmentedOption<OnboardMode>[] = [
  { value: 'off', label: 'Off' },
  { value: 'on', label: 'On' },
  { value: 'heartbeat', label: 'Heartbeat' },
  { value: 'blinkOnPatternChange', label: 'Blink on change' },
];

const percent = (value: number) => `${value}%`;

function duration(ms: number): string {
  if (ms < 1000) {
    return `${ms} ms`;
  }
  // At most two decimals, with no trailing zeros: 1 s, 1.5 s, 1.25 s.
  return `${Number((ms / 1000).toFixed(2))} s`;
}

const leds = (count: number) => (count === 1 ? '1 LED' : `${count} LEDs`);

/** Controls for the LED ring and the onboard LED. */
export function EyeScreen({ eye }: { eye: Eye }) {
  const { state, status, set } = eye;

  if (state === null) {
    return <p className="placeholder">{status === 'reconnecting' ? 'Reconnecting to the eye…' : 'Contacting the eye…'}</p>;
  }

  const offline = status !== 'live';
  const { comet, flash, blend, onboard } = state;
  const combined = state.pattern === 'blend';
  const blendCycleMs = blend.cometMs + blend.flashMs + 2 * blend.crossfadeMs;
  const flashBusyMs = flash.rampUpMs + flash.durationMs + flash.rampDownMs;

  return (
    <>
      <Card title="Ring" aside={<span className={state.running ? 'tag on' : 'tag'}>{state.running ? 'Running' : 'Stopped'}</span>}>
        <button
          type="button"
          className={state.running ? 'button power on' : 'button power'}
          role="switch"
          aria-checked={state.running}
          disabled={offline}
          onClick={() => set({ running: !state.running })}
        >
          {state.running ? 'Stop the ring' : 'Start the ring'}
        </button>

        <Segmented
          label="Pattern"
          value={state.pattern}
          options={state.patterns.map((pattern) => ({ value: pattern.name, label: pattern.label }))}
          disabled={offline}
          onChange={(pattern) => set({ pattern })}
        />

        <Slider
          label="Overall brightness"
          value={state.brightness}
          min={LIMITS.percent.min}
          max={LIMITS.percent.max}
          format={percent}
          disabled={offline}
          onChange={(brightness) => set({ brightness })}
        />
      </Card>

      {combined && (
        <Card title="Comet and flash">
          <Slider
            label="Time on comet"
            value={blend.cometMs}
            min={LIMITS.blendHoldMs.min}
            max={LIMITS.blendHoldMs.max}
            step={500}
            scale="fine-low"
            format={duration}
            disabled={offline}
            onChange={(cometMs) => set({ blend: { cometMs } })}
          />
          <Slider
            label="Time on flash"
            value={blend.flashMs}
            min={LIMITS.blendHoldMs.min}
            max={LIMITS.blendHoldMs.max}
            step={500}
            scale="fine-low"
            format={duration}
            disabled={offline}
            onChange={(flashMs) => set({ blend: { flashMs } })}
          />
          <Slider
            label="Crossfade"
            value={blend.crossfadeMs}
            min={LIMITS.blendCrossfadeMs.min}
            max={LIMITS.blendCrossfadeMs.max}
            step={100}
            format={(ms) => (ms === 0 ? 'Cut' : duration(ms))}
            hint={`One full round, there and back, takes ${duration(blendCycleMs)}.`}
            disabled={offline}
            onChange={(crossfadeMs) => set({ blend: { crossfadeMs } })}
          />
          <Segmented
            label="Comet direction"
            value={blend.randomDirection ? 'on' : 'off'}
            options={RANDOM_DIRECTION}
            disabled={offline}
            onChange={(choice) => set({ blend: { randomDirection: choice === 'on' } })}
          />
          <p className="hint">
            {blend.randomDirection
              ? 'The direction is picked afresh each time the comet returns, while the flash hides the change.'
              : 'The comet always turns the way chosen under Comet below.'}
          </p>
        </Card>
      )}

      {(state.pattern === 'comet' || combined) && (
        <Card title="Comet">
          {!(combined && blend.randomDirection) && (
            <Segmented
              label="Direction"
              value={comet.direction}
              options={DIRECTIONS}
              disabled={offline}
              onChange={(direction) => set({ comet: { direction } })}
            />
          )}
          <Slider
            label="Speed"
            value={comet.stepIntervalMs}
            min={LIMITS.cometStepMs.min}
            max={LIMITS.cometStepMs.max}
            step={5}
            scale="fine-low"
            reversed
            format={(ms) => `${duration(ms)} per LED`}
            hint={`One lap of the ring takes ${duration(comet.stepIntervalMs * 8)}.`}
            disabled={offline}
            onChange={(stepIntervalMs) => set({ comet: { stepIntervalMs } })}
          />
          <Slider
            label="Head brightness"
            value={comet.headBrightness}
            min={LIMITS.percent.min}
            max={LIMITS.percent.max}
            format={percent}
            disabled={offline}
            onChange={(headBrightness) => set({ comet: { headBrightness } })}
          />
          <Slider
            label="Tail length"
            value={comet.tailLength}
            min={LIMITS.cometTail.min}
            max={LIMITS.cometTail.max}
            format={leds}
            hint={`${leds(comet.tailLength + 1)} lit at a time, counting the head.`}
            disabled={offline}
            onChange={(tailLength) => set({ comet: { tailLength } })}
          />
          <Slider
            label="Tail fade"
            value={comet.tailFalloff}
            min={LIMITS.cometFalloff.min}
            max={LIMITS.cometFalloff.max}
            format={(value) => `${value}% dimmer per LED`}
            disabled={offline}
            onChange={(tailFalloff) => set({ comet: { tailFalloff } })}
          />
        </Card>
      )}

      {(state.pattern === 'flash' || combined) && (
        <Card title="Flash">
          <Slider
            label="Interval"
            value={flash.intervalMs}
            min={LIMITS.flashIntervalMs.min}
            max={LIMITS.flashIntervalMs.max}
            step={50}
            scale="fine-low"
            format={duration}
            hint="From the start of one flash to the start of the next."
            disabled={offline}
            onChange={(intervalMs) => set({ flash: { intervalMs } })}
          />
          <Slider
            label="Ramp up"
            value={flash.rampUpMs}
            min={LIMITS.flashRampMs.min}
            max={LIMITS.flashRampMs.max}
            step={10}
            scale="fine-low"
            format={(ms) => (ms === 0 ? 'Abrupt' : duration(ms))}
            disabled={offline}
            onChange={(rampUpMs) => set({ flash: { rampUpMs } })}
          />
          <Slider
            label="Hold at maximum"
            value={flash.durationMs}
            min={LIMITS.flashDurationMs.min}
            max={LIMITS.flashDurationMs.max}
            step={10}
            scale="fine-low"
            format={duration}
            disabled={offline}
            onChange={(durationMs) => set({ flash: { durationMs } })}
          />
          <Slider
            label="Ramp down"
            value={flash.rampDownMs}
            min={LIMITS.flashRampMs.min}
            max={LIMITS.flashRampMs.max}
            step={10}
            scale="fine-low"
            format={(ms) => (ms === 0 ? 'Abrupt' : duration(ms))}
            hint={`Ramp up, hold and ramp down use ${duration(flashBusyMs)} of the ${duration(flash.intervalMs)} interval. If they do not fit, the eye shortens the hold first.`}
            disabled={offline}
            onChange={(rampDownMs) => set({ flash: { rampDownMs } })}
          />
          <Slider
            label="Maximum brightness"
            value={flash.maxBrightness}
            min={LIMITS.percent.min}
            max={LIMITS.percent.max}
            format={percent}
            disabled={offline}
            onChange={(maxBrightness) => set({ flash: { maxBrightness } })}
          />
          <Slider
            label="Minimum brightness"
            value={flash.minBrightness}
            min={LIMITS.percent.min}
            max={LIMITS.percent.max}
            format={percent}
            hint="Cannot be higher than the maximum."
            disabled={offline}
            onChange={(minBrightness) => set({ flash: { minBrightness } })}
          />
        </Card>
      )}

      <Card title="Onboard LED">
        {state.onboardAvailable ? (
          <>
            <Segmented
              label="Behaviour"
              value={onboard.mode}
              options={ONBOARD_MODES}
              twoColumns
              disabled={offline}
              onChange={(mode) => set({ onboard: { mode } })}
            />
            {onboard.mode !== 'off' && (
              <Slider
                label="Brightness"
                value={onboard.brightness}
                min={LIMITS.percent.min}
                max={LIMITS.percent.max}
                format={percent}
                disabled={offline}
                onChange={(brightness) => set({ onboard: { brightness } })}
              />
            )}
            {onboard.mode === 'blinkOnPatternChange' && (
              <>
                <Slider
                  label="Blinks"
                  value={onboard.blinkCount}
                  min={LIMITS.onboardBlinkCount.min}
                  max={LIMITS.onboardBlinkCount.max}
                  format={String}
                  disabled={offline}
                  onChange={(blinkCount) => set({ onboard: { blinkCount } })}
                />
                <Slider
                  label="Blink length"
                  value={onboard.blinkMs}
                  min={LIMITS.onboardBlinkMs.min}
                  max={LIMITS.onboardBlinkMs.max}
                  step={10}
                  format={duration}
                  hint="Blinks when the ring pattern is switched."
                  disabled={offline}
                  onChange={(blinkMs) => set({ onboard: { blinkMs } })}
                />
              </>
            )}
          </>
        ) : (
          <p className="hint">This board has no onboard LED that the eye can control.</p>
        )}
      </Card>

      <div className="actions">
        <ConfirmButton
          label="Reset LED settings"
          confirmLabel="Tap again to reset everything"
          disabled={offline}
          onConfirm={eye.reset}
        />
      </div>
    </>
  );
}
