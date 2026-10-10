"""POST /api/config with valid values."""
import pytest


def test_response_is_the_resulting_state(device):
    result = device.set({"brightness": 25})
    assert result["brightness"] == 25
    assert result == device.state()


def test_partial_update_changes_only_what_is_named(device, defaults):
    result = device.set({"comet": {"tailLength": 2}})

    expected = dict(defaults)
    expected["comet"] = dict(defaults["comet"], tailLength=2)
    assert result == expected


def test_empty_object_changes_nothing(device, defaults):
    assert device.set({}) == defaults


def test_unknown_keys_are_ignored(device, defaults):
    result = device.set({"sparkle": 1, "comet": {"wobble": True}})
    assert result == defaults


@pytest.mark.parametrize("pattern", ["flash", "blend", "comet"])
def test_pattern_can_be_selected(device, pattern):
    assert device.set({"pattern": pattern})["pattern"] == pattern


def test_ring_can_be_stopped_and_started(device):
    assert device.set({"running": False})["running"] is False
    assert device.set({"running": True})["running"] is True


def test_every_comet_setting_can_be_changed(device):
    values = {
        "direction": "counterClockwise",
        "stepIntervalMs": 250,
        "headBrightness": 80,
        "tailLength": 6,
        "tailFalloff": 30,
    }
    assert device.set({"comet": values})["comet"] == values


def test_every_flash_setting_can_be_changed(device):
    values = {
        "intervalMs": 2500,
        "durationMs": 300,
        "minBrightness": 5,
        "maxBrightness": 95,
        "rampUpMs": 400,
        "rampDownMs": 600,
    }
    assert device.set({"flash": values})["flash"] == values


def test_every_combined_pattern_setting_can_be_changed(device):
    values = {
        "cometMs": 12000,
        "flashMs": 3000,
        "crossfadeMs": 750,
        "randomDirection": False,
    }
    assert device.set({"blend": values})["blend"] == values


def test_combined_pattern_leaves_the_comet_and_flash_settings_alone(device, defaults):
    result = device.set({"pattern": "blend", "blend": {"crossfadeMs": 0}})
    assert result["pattern"] == "blend"
    assert result["comet"] == defaults["comet"]
    assert result["flash"] == defaults["flash"]


def test_comet_and_flash_can_be_adjusted_while_combined(device):
    device.set({"pattern": "blend"})
    result = device.set({"comet": {"stepIntervalMs": 150}, "flash": {"intervalMs": 700}})
    assert result["pattern"] == "blend"
    assert result["comet"]["stepIntervalMs"] == 150
    assert result["flash"]["intervalMs"] == 700


def test_every_onboard_setting_can_be_changed(device):
    values = {"mode": "heartbeat", "brightness": 20, "blinkCount": 4, "blinkMs": 80}
    assert device.set({"onboard": values})["onboard"] == values


@pytest.mark.parametrize("mode", ["off", "on", "heartbeat", "blinkOnPatternChange"])
def test_each_onboard_mode_is_accepted(device, mode):
    assert device.set({"onboard": {"mode": mode}})["onboard"]["mode"] == mode


def test_several_groups_in_one_request(device):
    result = device.set(
        {
            "pattern": "flash",
            "brightness": 40,
            "flash": {"intervalMs": 500, "durationMs": 100},
            "onboard": {"mode": "off"},
        }
    )
    assert result["pattern"] == "flash"
    assert result["brightness"] == 40
    assert result["flash"]["intervalMs"] == 500
    assert result["flash"]["durationMs"] == 100
    assert result["onboard"]["mode"] == "off"


def test_fractional_numbers_are_rounded(device):
    assert device.set({"brightness": 41.6})["brightness"] == 42


def test_rapid_changes_all_land(device):
    """What a slider drag over plain HTTP looks like."""
    for value in range(0, 101, 5):
        assert device.set({"brightness": value})["brightness"] == value
    assert device.state()["brightness"] == 100
