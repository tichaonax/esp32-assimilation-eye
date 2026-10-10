"""PlatformIO pre-build script: LED defaults for this build.

If led-defaults.json exists next to platformio.ini, its settings become the
defaults the firmware is built with: what a board that has never been set up
shows, and what "Reset LED settings" and a factory reset return to. Without
the file the built-in defaults in lib/eye_core/EyeConfig.cpp are used.

The file holds any subset of the LED settings, in the shape the API uses:

    {
      "pattern": "blend",
      "brightness": 80,
      "comet": { "stepIntervalMs": 70, "tailLength": 5 }
    }

scripts/save_led_defaults.py writes it from the settings on a board.

The file is checked here, and a mistake stops the build with a message that
names the setting. Ranges and pattern names are read from the firmware's own
source, so this script does not need editing when they change.

It writes lib/eye_core/generated/led_defaults.h, which only
lib/eye_core/FactoryConfig.cpp includes.
"""
import json
import os
import re

FILE_NAME = "led-defaults.json"

DIRECTIONS = ("clockwise", "counterClockwise")
ONBOARD_MODES = ("off", "on", "heartbeat", "blinkOnPatternChange")

# setting -> what it must be. A pair of names is a range from EyeConfig.h.
PERCENT = ("PERCENT_MIN", "PERCENT_MAX")
SETTINGS = {
    None: {
        "running": bool,
        "pattern": "pattern",
        "brightness": PERCENT,
    },
    "comet": {
        "direction": DIRECTIONS,
        "stepIntervalMs": ("COMET_STEP_MS_MIN", "COMET_STEP_MS_MAX"),
        "headBrightness": PERCENT,
        "tailLength": ("COMET_TAIL_MIN", "COMET_TAIL_MAX"),
        "tailFalloff": ("COMET_FALLOFF_MIN", "COMET_FALLOFF_MAX"),
    },
    "flash": {
        "intervalMs": ("FLASH_INTERVAL_MS_MIN", "FLASH_INTERVAL_MS_MAX"),
        "durationMs": ("FLASH_DURATION_MS_MIN", "FLASH_DURATION_MS_MAX"),
        "minBrightness": PERCENT,
        "maxBrightness": PERCENT,
        "rampUpMs": ("FLASH_RAMP_MS_MIN", "FLASH_RAMP_MS_MAX"),
        "rampDownMs": ("FLASH_RAMP_MS_MIN", "FLASH_RAMP_MS_MAX"),
    },
    "blend": {
        "cometMs": ("BLEND_HOLD_MS_MIN", "BLEND_HOLD_MS_MAX"),
        "flashMs": ("BLEND_HOLD_MS_MIN", "BLEND_HOLD_MS_MAX"),
        "crossfadeMs": ("BLEND_CROSSFADE_MS_MIN", "BLEND_CROSSFADE_MS_MAX"),
        "randomDirection": bool,
    },
    "onboard": {
        "mode": ONBOARD_MODES,
        "brightness": PERCENT,
        "blinkCount": ("ONBOARD_BLINK_COUNT_MIN", "ONBOARD_BLINK_COUNT_MAX"),
        "blinkMs": ("ONBOARD_BLINK_MS_MIN", "ONBOARD_BLINK_MS_MAX"),
    },
}


class DefaultsError(Exception):
    """A problem with led-defaults.json, worded for the person who wrote it."""


def read_limits(project_dir):
    """The ranges in lib/eye_core/EyeConfig.h, by name."""
    path = os.path.join(project_dir, "lib", "eye_core", "EyeConfig.h")
    with open(path, encoding="utf-8") as handle:
        source = handle.read()

    led_count = re.search(r"constexpr size_t LED_COUNT = (\d+);", source)
    limits = {}
    for name, value in re.findall(r"constexpr int32_t (\w+) = ([^;]+);", source):
        value = value.strip()
        if re.fullmatch(r"-?\d+", value):
            limits[name] = int(value)
        elif value == "static_cast<int32_t>(LED_COUNT) - 1" and led_count:
            limits[name] = int(led_count.group(1)) - 1
        else:
            raise DefaultsError(f"scripts/led_defaults.py cannot work out {name} = {value} in EyeConfig.h")

    needed = {name for group in SETTINGS.values() for kind in group.values() if _is_range(kind) for name in kind}
    missing = sorted(needed - limits.keys())
    if missing:
        raise DefaultsError("scripts/led_defaults.py did not find these ranges in EyeConfig.h: " + ", ".join(missing))
    return limits


def read_pattern_names(project_dir):
    """The identifier of every pattern class in lib/eye_core."""
    folder = os.path.join(project_dir, "lib", "eye_core")
    names = []
    for entry in sorted(os.listdir(folder)):
        if entry.endswith("Pattern.h"):
            with open(os.path.join(folder, entry), encoding="utf-8") as handle:
                names += re.findall(r'const char\* name\(\) const override \{ return "([^"]+)"; \}', handle.read())
    if not names:
        raise DefaultsError("scripts/led_defaults.py found no patterns in lib/eye_core")
    return names


def _is_range(kind):
    return isinstance(kind, tuple) and len(kind) == 2 and kind[0].endswith("_MIN")


def _check_value(label, value, kind, limits, patterns):
    if kind is bool:
        if not isinstance(value, bool):
            raise DefaultsError(f"{label} must be true or false, not {json.dumps(value)}")
    elif kind == "pattern":
        if not isinstance(value, str) or value not in patterns:
            raise DefaultsError(f"{label} must be one of {', '.join(patterns)}, not {json.dumps(value)}")
    elif _is_range(kind):
        low, high = limits[kind[0]], limits[kind[1]]
        # bool is a kind of int in Python, and true is not a brightness.
        if isinstance(value, bool) or not isinstance(value, int):
            raise DefaultsError(f"{label} must be a whole number from {low} to {high}, not {json.dumps(value)}")
        if value < low or value > high:
            raise DefaultsError(f"{label} is {value}; it must be from {low} to {high}")
    else:
        if not isinstance(value, str) or value not in kind:
            raise DefaultsError(f"{label} must be one of {', '.join(kind)}, not {json.dumps(value)}")


def check(settings, limits, patterns):
    """Raises DefaultsError for the first problem found in the parsed file."""
    if not isinstance(settings, dict):
        raise DefaultsError("the file must hold one JSON object, { ... }")

    for key, value in settings.items():
        if key in SETTINGS[None]:
            _check_value(key, value, SETTINGS[None][key], limits, patterns)
        elif key in SETTINGS:
            if not isinstance(value, dict):
                raise DefaultsError(f"{key} must be an object, {{ ... }}")
            for name, inner in value.items():
                if name not in SETTINGS[key]:
                    known = ", ".join(SETTINGS[key])
                    raise DefaultsError(f"{key}.{name} is not a setting. The {key} settings are: {known}")
                _check_value(f"{key}.{name}", inner, SETTINGS[key][name], limits, patterns)
        else:
            known = ", ".join(name for name in list(SETTINGS[None]) + [g for g in SETTINGS if g])
            raise DefaultsError(f"{key} is not a setting. The settings are: {known}")

    # Rules between settings. The firmware would quietly adjust these; a
    # default that is not what was written is better caught here. They can
    # only be judged when every value involved is in the file.
    flash = settings.get("flash", {})
    if "minBrightness" in flash and "maxBrightness" in flash and flash["minBrightness"] > flash["maxBrightness"]:
        raise DefaultsError(
            f"flash.minBrightness ({flash['minBrightness']}) is higher than flash.maxBrightness ({flash['maxBrightness']})"
        )
    timing = ("intervalMs", "rampUpMs", "durationMs", "rampDownMs")
    if all(name in flash for name in timing):
        busy = flash["rampUpMs"] + flash["durationMs"] + flash["rampDownMs"]
        if busy > flash["intervalMs"]:
            raise DefaultsError(
                f"flash.rampUpMs + flash.durationMs + flash.rampDownMs is {busy} ms, "
                f"which does not fit in flash.intervalMs ({flash['intervalMs']} ms)"
            )


def load(project_dir):
    """The parsed, checked file, or None if there is no file."""
    path = os.path.join(project_dir, FILE_NAME)
    if not os.path.isfile(path):
        return None
    try:
        with open(path, encoding="utf-8-sig") as handle:
            settings = json.load(handle)
    except json.JSONDecodeError as error:
        raise DefaultsError(f"not valid JSON: {error.msg} at line {error.lineno}, column {error.colno}") from None
    except UnicodeDecodeError:
        raise DefaultsError("not a UTF-8 text file") from None
    check(settings, read_limits(project_dir), read_pattern_names(project_dir))
    return settings


def header_text(settings):
    """The generated header for these settings; None means no file."""
    lines = [
        "// Generated by scripts/led_defaults.py from led-defaults.json. Do not edit; do not commit.",
        "#pragma once",
    ]
    if settings is not None:
        compact = json.dumps(settings, separators=(",", ":"), ensure_ascii=True)
        # json.dumps output is a valid C string literal once quoted again.
        lines.append(f"#define EYE_LED_DEFAULTS_JSON {json.dumps(compact)}")
    return "\n".join(lines) + "\n"


def describe(settings):
    """One line saying what the file sets, for the build log."""
    parts = []
    for key, value in settings.items():
        if isinstance(value, dict):
            parts += [f"{key}.{name}" for name in value]
        else:
            parts.append(key)
    return f"{len(parts)} setting{'' if len(parts) == 1 else 's'} from {FILE_NAME}"


def write_header(project_dir, settings):
    """Writes the header only if it would change, so nothing is recompiled for no reason."""
    path = os.path.join(project_dir, "lib", "eye_core", "generated", "led_defaults.h")
    text = header_text(settings)
    try:
        with open(path, encoding="utf-8", newline="") as handle:
            if handle.read() == text:
                return False
    except OSError:
        pass
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
    return True


def _pre_build(env):
    project_dir = env.subst("$PROJECT_DIR")
    try:
        settings = load(project_dir)
    except DefaultsError as error:
        print(f"led_defaults: {FILE_NAME}: {error}")
        print("led_defaults: build stopped. Correct the file, or delete it to build with the built-in defaults.")
        env.Exit(1)
        return
    write_header(project_dir, settings)
    if settings is None:
        print(f"led_defaults: no {FILE_NAME}; using the built-in LED defaults")
    else:
        print(f"led_defaults: CUSTOM LED DEFAULTS - {describe(settings)}")


try:
    Import("env")  # noqa: F821  (provided by PlatformIO)
except NameError:
    pass  # imported by the helper script or the tests, not run by a build
else:
    if not env.IsIntegrationDump():  # noqa: F821
        _pre_build(env)  # noqa: F821
