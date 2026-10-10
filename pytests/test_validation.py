"""POST /api/config with values that must be clamped or refused."""
import pytest

# (group, setting, minimum, maximum). group None = top level.
RANGES = [
    (None, "brightness", 0, 100),
    ("comet", "stepIntervalMs", 20, 2000),
    ("comet", "headBrightness", 0, 100),
    ("comet", "tailLength", 0, 7),
    ("comet", "tailFalloff", 10, 90),
    ("flash", "intervalMs", 100, 10000),
    ("flash", "minBrightness", 0, 100),
    ("flash", "maxBrightness", 0, 100),
    ("blend", "cometMs", 1000, 60000),
    ("blend", "flashMs", 1000, 60000),
    ("blend", "crossfadeMs", 0, 5000),
    ("onboard", "brightness", 0, 100),
    ("onboard", "blinkCount", 1, 5),
    ("onboard", "blinkMs", 50, 500),
]

IDS = [f"{group}.{name}" if group else name for group, name, _, _ in RANGES]


def wrap(group, name, value):
    return {group: {name: value}} if group else {name: value}


def pick(state, group, name):
    return state[group][name] if group else state[name]


@pytest.mark.parametrize("group,name,low,high", RANGES, ids=IDS)
def test_value_below_range_is_raised_to_the_minimum(device, group, name, low, high):
    if name == "minBrightness":
        pytest.skip("covered by the min/max test")
    result = device.set(wrap(group, name, low - 1000))
    assert pick(result, group, name) == low


@pytest.mark.parametrize("group,name,low,high", RANGES, ids=IDS)
def test_value_above_range_is_lowered_to_the_maximum(device, group, name, low, high):
    result = device.set(wrap(group, name, high + 100000))
    assert pick(result, group, name) == high


@pytest.mark.parametrize("group,name,low,high", RANGES, ids=IDS)
def test_range_limits_themselves_are_accepted(device, group, name, low, high):
    assert pick(device.set(wrap(group, name, high)), group, name) == high
    if name != "maxBrightness":  # lowering the maximum also lowers the minimum
        assert pick(device.set(wrap(group, name, low)), group, name) == low


def test_absurdly_large_numbers_are_clamped(device):
    result = device.set({"brightness": 1e30, "comet": {"stepIntervalMs": -1e30}})
    assert result["brightness"] == 100
    assert result["comet"]["stepIntervalMs"] == 20


def test_flash_minimum_is_forced_down_to_the_maximum(device):
    result = device.set({"flash": {"minBrightness": 80, "maxBrightness": 30}})
    assert result["flash"]["minBrightness"] == 30
    assert result["flash"]["maxBrightness"] == 30


def test_flash_hold_is_shortened_to_fit_the_interval(device):
    result = device.set({"flash": {"intervalMs": 1000, "rampUpMs": 300, "durationMs": 900, "rampDownMs": 200}})
    assert result["flash"]["rampUpMs"] == 300
    assert result["flash"]["durationMs"] == 500
    assert result["flash"]["rampDownMs"] == 200


def test_flash_ramps_are_scaled_when_they_alone_do_not_fit(device):
    result = device.set({"flash": {"intervalMs": 1000, "rampUpMs": 3000, "durationMs": 200, "rampDownMs": 1000}})
    assert result["flash"]["rampUpMs"] == 750
    assert result["flash"]["durationMs"] == 0
    assert result["flash"]["rampDownMs"] == 250


def test_flash_ramp_limit_is_5000_ms(device):
    result = device.set({"flash": {"intervalMs": 10000, "durationMs": 0, "rampUpMs": 9000, "rampDownMs": 9000}})
    assert result["flash"]["rampUpMs"] == 5000
    assert result["flash"]["rampDownMs"] == 5000


def test_shortening_the_interval_refits_the_timing(device):
    device.set({"flash": {"intervalMs": 2000, "rampUpMs": 500, "durationMs": 1000, "rampDownMs": 500}})
    flash = device.set({"flash": {"intervalMs": 1200}})["flash"]
    assert flash["rampUpMs"] + flash["durationMs"] + flash["rampDownMs"] <= 1200


BAD_VALUES = [
    {"brightness": "bright"},
    {"brightness": True},
    {"brightness": [50]},
    {"running": 1},
    {"running": "yes"},
    {"pattern": 5},
    {"pattern": ""},
    {"pattern": "sparkle"},
    {"pattern": "a-very-long-pattern-name-indeed"},
    {"comet": 7},
    {"comet": {"tailLength": "long"}},
    {"comet": {"direction": "sideways"}},
    {"comet": {"direction": 1}},
    {"flash": [1, 2]},
    {"flash": {"intervalMs": {}}},
    {"blend": "both"},
    {"blend": {"cometMs": "long"}},
    {"blend": {"randomDirection": 1}},
    {"blend": {"randomDirection": "yes"}},
    {"onboard": {"mode": "disco"}},
    {"onboard": {"mode": False}},
    {"onboard": {"blinkCount": "two"}},
]


@pytest.mark.parametrize("values", BAD_VALUES, ids=[str(v) for v in BAD_VALUES])
def test_bad_value_is_refused_with_a_message_and_changes_nothing(device, defaults, values):
    response = device.post("/api/config", values)

    assert response.status_code == 400
    assert response.json()["error"]
    assert device.state() == defaults


def test_good_values_alongside_a_bad_one_are_not_applied(device, defaults):
    response = device.post("/api/config", {"brightness": 5, "onboard": {"blinkCount": "two"}})
    assert response.status_code == 400
    assert device.state() == defaults


def test_error_message_names_the_setting(device):
    response = device.post("/api/config", {"comet": {"tailLength": "long"}})
    assert response.json()["error"] == "comet.tailLength must be a number"


@pytest.mark.parametrize("body", ["[1, 2, 3]", '"comet"', "42", "null"])
def test_body_that_is_not_an_object_is_refused(device, defaults, body):
    response = device.post("/api/config", data=body, headers={"Content-Type": "application/json"})
    assert response.status_code == 400
    assert device.state() == defaults


@pytest.mark.parametrize("body", ["{", '{"brightness": }', "brightness=5", "\x00\x01\x02"])
def test_body_that_is_not_json_is_refused(device, defaults, body):
    response = device.post("/api/config", data=body, headers={"Content-Type": "application/json"})
    assert response.status_code == 400
    assert "error" in response.json()
    assert device.state() == defaults


def test_missing_body_is_refused(device):
    response = device.post("/api/config")
    assert response.status_code == 400


def test_oversized_body_is_refused(device, defaults):
    body = '{"brightness": 5, "padding": "' + "x" * 4000 + '"}'
    response = device.post("/api/config", data=body, headers={"Content-Type": "application/json"})
    assert response.status_code == 413
    assert device.state() == defaults


def test_board_still_works_after_a_run_of_bad_requests(device, defaults):
    for _ in range(25):
        device.post("/api/config", data="{", headers={"Content-Type": "application/json"})
    assert device.set({"brightness": 33})["brightness"] == 33
