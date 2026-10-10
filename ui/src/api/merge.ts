import type { EyeState, SettingsPatch } from './types';

/** Combines two patches; values in `later` win. */
export function mergePatches(earlier: SettingsPatch | null, later: SettingsPatch): SettingsPatch {
  if (earlier === null) {
    return later;
  }
  return {
    ...earlier,
    ...later,
    ...(earlier.comet || later.comet ? { comet: { ...earlier.comet, ...later.comet } } : {}),
    ...(earlier.flash || later.flash ? { flash: { ...earlier.flash, ...later.flash } } : {}),
    ...(earlier.blend || later.blend ? { blend: { ...earlier.blend, ...later.blend } } : {}),
    ...(earlier.onboard || later.onboard ? { onboard: { ...earlier.onboard, ...later.onboard } } : {}),
  };
}

/** The state with a patch laid over it. */
export function applyPatch(state: EyeState, patch: SettingsPatch | null): EyeState {
  if (patch === null) {
    return state;
  }
  return {
    ...state,
    ...patch,
    comet: { ...state.comet, ...patch.comet },
    flash: { ...state.flash, ...patch.flash },
    blend: { ...state.blend, ...patch.blend },
    onboard: { ...state.onboard, ...patch.onboard },
  };
}
