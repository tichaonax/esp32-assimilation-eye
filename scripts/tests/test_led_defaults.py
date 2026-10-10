"""scripts/led_defaults.py: what it accepts, what it refuses and what it generates.

No board needed:

    pytests/.venv/Scripts/python -m pytest scripts/tests
"""
import json
import os
import shutil
import sys

import pytest

SCRIPTS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT = os.path.dirname(SCRIPTS)
sys.path.insert(0, SCRIPTS)

import led_defaults  # noqa: E402
import save_led_defaults  # noqa: E402

LIMITS = led_defaults.read_limits(PROJECT)
PATTERNS = led_defaults.read_pattern_names(PROJECT)

# Everything the board reports, as save_led_defaults.py would write it.
COMPLETE = {
    "running": True,
    "pattern": "blend",
    "brightness": 80,
    "comet": {"direction": "counterClockwise", "stepIntervalMs": 70, "headBrightness": 100, "tailLength": 5, "tailFalloff": 40},
    "flash": {"intervalMs": 1200, "durationMs": 150, "minBrightness": 5, "maxBrightness": 100, "rampUpMs": 200, "rampDownMs": 400},
    "blend": {"cometMs": 10000, "flashMs": 4000, "crossfadeMs": 2000, "randomDirection": True},
    "onboard": {"mode": "heartbeat", "brightness": 30, "blinkCount": 2, "blinkMs": 120},
}


def check(settings):
    led_defaults.check(settings, LIMITS, PATTERNS)


def refusal(settings):
    with pytest.raises(led_defaults.DefaultsError) as caught:
        check(settings)
    return str(caught.value)


@pytest.fixture
def project(tmp_path):
    """A copy of just enough of the source tree for the script to read."""
    core = tmp_path / "lib" / "eye_core"
    core.mkdir(parents=True)
    for name in os.listdir(os.path.join(PROJECT, "lib", "eye_core")):
        if name == "EyeConfig.h" or name.endswith("Pattern.h"):
            shutil.copy(os.path.join(PROJECT, "lib", "eye_core", name), core / name)
    return tmp_path


def write(project, text):
    (project / led_defaults.FILE_NAME).write_text(text, encoding="utf-8")


# ---- What the firmware source says ---------------------------------------------------


def test_ranges_are_read_from_the_firmware_source():
    assert LIMITS["PERCENT_MAX"] == 100
    assert LIMITS["COMET_STEP_MS_MIN"] == 20
    assert LIMITS["COMET_TAIL_MAX"] == 7, "one less than the number of LEDs"
    assert LIMITS["BLEND_HOLD_MS_MAX"] == 60000


def test_pattern_names_are_read_from_the_firmware_source():
    assert set(PATTERNS) == {"comet", "flash", "blend"}


def test_every_setting_the_board_reports_is_known_here():
    """A setting added to the firmware but not to the script would make the helper's file fail the build."""
    source = open(os.path.join(PROJECT, "lib", "eye_core", "ConfigJson.cpp"), encoding="utf-8").read()
    written = source[source.index("void write(") : source.index("bool read(")]
    for group, names in led_defaults.SETTINGS.items():
        for name in names:
            assert f'["{name}"]' in written, f"{group}.{name} is not written by ConfigJson.cpp"
    import re

    in_firmware = set(re.findall(r'\["(\w+)"\] =', written))
    known = {name for names in led_defaults.SETTINGS.values() for name in names} | {g for g in led_defaults.SETTINGS if g}
    assert in_firmware <= known, f"not in scripts/led_defaults.py: {sorted(in_firmware - known)}"


# ---- Accepted ------------------------------------------------------------------------


def test_complete_settings_are_accepted():
    check(COMPLETE)


def test_empty_object_is_accepted():
    check({})


@pytest.mark.parametrize(
    "settings",
    [
        {"brightness": 0},
        {"brightness": 100},
        {"pattern": "flash"},
        {"running": False},
        {"comet": {}},
        {"comet": {"tailLength": 7}},
        {"blend": {"crossfadeMs": 0, "randomDirection": False}},
        {"flash": {"rampUpMs": 5000}},  # too long for the default interval, but that cannot be judged from here
    ],
    ids=str,
)
def test_any_subset_within_range_is_accepted(settings):
    check(settings)


# ---- Refused, naming the setting -----------------------------------------------------


@pytest.mark.parametrize(
    "settings,expected",
    [
        ({"brightness": 101}, "brightness is 101; it must be from 0 to 100"),
        ({"brightness": -1}, "brightness is -1"),
        ({"brightness": "bright"}, "brightness must be a whole number from 0 to 100"),
        ({"brightness": 50.5}, "brightness must be a whole number"),
        ({"brightness": True}, "brightness must be a whole number"),
        ({"running": 1}, "running must be true or false"),
        ({"pattern": "sparkle"}, "pattern must be one of"),
        ({"pattern": 3}, "pattern must be one of"),
        ({"comet": {"stepIntervalMs": 5}}, "comet.stepIntervalMs is 5; it must be from 20 to 2000"),
        ({"comet": {"tailLength": 8}}, "comet.tailLength is 8; it must be from 0 to 7"),
        ({"comet": {"direction": "sideways"}}, "comet.direction must be one of clockwise, counterClockwise"),
        ({"comet": {"speed": 3}}, "comet.speed is not a setting"),
        ({"comet": 5}, "comet must be an object"),
        ({"flash": {"intervalMs": 20000}}, "flash.intervalMs is 20000"),
        ({"blend": {"cometMs": 500}}, "blend.cometMs is 500; it must be from 1000 to 60000"),
        ({"blend": {"randomDirection": "yes"}}, "blend.randomDirection must be true or false"),
        ({"onboard": {"mode": "disco"}}, "onboard.mode must be one of"),
        ({"onboard": {"blinkCount": 9}}, "onboard.blinkCount is 9"),
        ({"sparkle": 1}, "sparkle is not a setting"),
        ({"patterns": []}, "patterns is not a setting"),
        ([1, 2], "must hold one JSON object"),
    ],
    ids=lambda value: str(value) if not isinstance(value, str) else None,
)
def test_bad_value_is_refused_naming_the_setting(settings, expected):
    assert expected in refusal(settings)


def test_flash_minimum_above_maximum_is_refused():
    assert "flash.minBrightness (80) is higher than flash.maxBrightness (30)" in refusal(
        {"flash": {"minBrightness": 80, "maxBrightness": 30}}
    )


def test_flash_timing_that_does_not_fit_is_refused():
    message = refusal({"flash": {"intervalMs": 1000, "rampUpMs": 300, "durationMs": 900, "rampDownMs": 200}})
    assert "1400 ms" in message and "flash.intervalMs (1000 ms)" in message


# ---- The file ------------------------------------------------------------------------


def test_no_file_means_no_custom_defaults(project):
    assert led_defaults.load(str(project)) is None


def test_file_is_loaded_and_checked(project):
    write(project, json.dumps(COMPLETE))
    assert led_defaults.load(str(project)) == COMPLETE


def test_file_with_a_byte_order_mark_is_accepted(project):
    (project / led_defaults.FILE_NAME).write_bytes(b"\xef\xbb\xbf" + json.dumps({"brightness": 70}).encode())
    assert led_defaults.load(str(project)) == {"brightness": 70}


@pytest.mark.parametrize("text", ["", "{", '{"brightness": }', "brightness = 5", '{"brightness": 70,}'])
def test_file_that_is_not_json_is_refused_with_the_place(project, text):
    write(project, text)
    with pytest.raises(led_defaults.DefaultsError) as caught:
        led_defaults.load(str(project))
    assert "not valid JSON" in str(caught.value) and "line 1" in str(caught.value)


def test_bad_value_in_the_file_is_refused(project):
    write(project, '{"comet": {"tailLength": 12}}')
    with pytest.raises(led_defaults.DefaultsError) as caught:
        led_defaults.load(str(project))
    assert "comet.tailLength is 12" in str(caught.value)


# ---- The generated header ------------------------------------------------------------


def test_header_without_a_file_defines_nothing():
    text = led_defaults.header_text(None)
    assert "#pragma once" in text and "#define" not in text


def test_header_carries_the_settings_as_one_c_string():
    text = led_defaults.header_text({"pattern": "blend", "comet": {"tailLength": 5}})
    line = next(line for line in text.splitlines() if line.startswith("#define EYE_LED_DEFAULTS_JSON "))
    literal = line[len("#define EYE_LED_DEFAULTS_JSON ") :]
    # A C string literal with only \" and \\ escapes reads the same as a JSON string.
    assert json.loads(json.loads(literal)) == {"pattern": "blend", "comet": {"tailLength": 5}}
    assert "\n" not in literal


def test_header_is_rewritten_only_when_it_changes(project):
    assert led_defaults.write_header(str(project), {"brightness": 70}) is True
    assert led_defaults.write_header(str(project), {"brightness": 70}) is False
    assert led_defaults.write_header(str(project), {"brightness": 71}) is True
    assert led_defaults.write_header(str(project), None) is True
    assert led_defaults.write_header(str(project), None) is False


def test_build_log_line_counts_the_settings():
    assert led_defaults.describe({"pattern": "blend", "comet": {"tailLength": 5, "tailFalloff": 40}}) == (
        "3 settings from led-defaults.json"
    )
    assert led_defaults.describe({"brightness": 70}) == "1 setting from led-defaults.json"


# ---- The capture helper --------------------------------------------------------------


def test_helper_keeps_the_settings_and_drops_the_rest():
    state = dict(json.loads(json.dumps(COMPLETE)), patterns=[{"name": "comet", "label": "Rotating comet"}], onboardAvailable=True)
    state["comet"]["futureSetting"] = 1
    assert save_led_defaults.settings_from_state(state) == COMPLETE


def test_what_the_helper_writes_passes_the_build_check():
    check(save_led_defaults.settings_from_state(COMPLETE))


def test_helper_lists_settings_by_their_full_name():
    assert save_led_defaults.flatten({"brightness": 5, "comet": {"tailLength": 2}}) == {"brightness": 5, "comet.tailLength": 2}
