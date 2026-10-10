"""System: information, firmware upload checks, restart and factory reset."""
import os
import re
import time

import pytest

from conftest import expected_defaults

SLOT_BYTES = 1966080
UPLOAD_TIMEOUT = (2, 180)


def upload(device, content, name="firmware.bin"):
    return device.post(
        "/api/update", files={"firmware": (name, content, "application/octet-stream")}, timeout=UPLOAD_TIMEOUT
    )


def built_file(request, name):
    path = os.path.join(str(request.config.rootpath), ".pio", "build", "esp32dev", name)
    if not os.path.isfile(path):
        pytest.skip(f"{name} has not been built on this PC")
    with open(path, "rb") as handle:
        return handle.read()


# ---- Information --------------------------------------------------------------------


def test_system_reports_firmware_chip_memory_and_flash(device):
    info = device.json("/api/system")

    assert re.fullmatch(r"\d+\.\d+\.\d+", info["firmware"]["version"])
    assert re.fullmatch(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}", info["firmware"]["built"])
    assert isinstance(info["firmware"]["developmentBuild"], bool)
    assert isinstance(info["firmware"]["customLedDefaults"], bool)

    assert info["chip"]["model"].startswith("ESP32")
    assert info["chip"]["cores"] == 2
    assert len(info["chip"]["mac"].split(":")) == 6

    assert info["memory"]["freeBytes"] > 60_000, "free memory is getting low"
    assert info["memory"]["lowestFreeBytes"] <= info["memory"]["freeBytes"]

    assert info["flash"]["slotBytes"] == SLOT_BYTES
    assert 0 < info["flash"]["firmwareBytes"] < SLOT_BYTES
    assert info["flash"]["sizeBytes"] == 4 * 1024 * 1024
    assert info["resetReason"]


def test_uptime_counts_up(device):
    first = device.json("/api/system")["uptimeS"]
    time.sleep(2.2)
    assert device.json("/api/system")["uptimeS"] >= first + 2


def test_animation_keeps_its_rhythm_while_the_interface_is_busy(device):
    """AC 6: web traffic must not disturb the LEDs.

    The board counts frames and records the longest wait between two. A frame
    is due every 5 ms; a hitch the eye could see would be several times that.

    Kept under ten seconds and read straight away, so that no save to flash
    falls inside the measurement (see the next test).
    """
    time.sleep(2.6)  # let any save still pending from earlier tests happen first
    start = device.json("/api/system")["animation"]  # also clears the longest-wait record
    began = time.time()

    busy = device.anonymous()
    busy.login()  # a password check is the heaviest thing the web server does
    with busy.socket() as socket:
        socket.receive_state()
        value = 0
        while time.time() - began < 6:
            value += 1
            socket.send({"type": "set", "values": {"brightness": value % 100, "comet": {"tailLength": value % 8}}})
            device.set({"comet": {"stepIntervalMs": 50 + value % 100}})
            device.json("/api/wifi")
            socket.drain()
    busy.logout()

    end = device.json("/api/system")["animation"]
    seconds = time.time() - began
    assert seconds < 9, "the measurement ran long enough to include a save; shorten it"
    rate = (end["frames"] - start["frames"]) / seconds
    assert end["tickMs"] == 5
    assert 190 <= rate <= 210, f"{rate:.1f} frames per second"
    assert end["worstGapMs"] <= 15, f"a frame was {end['worstGapMs']} ms late"


def test_saving_settings_costs_at_most_one_short_pause(device):
    """Writing to flash stops everything on this chip for a moment.

    Most saves cost nothing measurable. About one in six has to erase a block
    of flash first, which holds a frame up by some tens of milliseconds. It
    happens two seconds after the last change, not while a control is moving.
    This test fixes the upper bound so a regression would show.
    """
    time.sleep(2.6)
    device.json("/api/system")
    pauses = []
    for value in range(6):
        device.set({"brightness": 20 + value})
        time.sleep(2.8)  # the save happens in here
        pauses.append(device.json("/api/system")["animation"]["worstGapMs"])
    assert max(pauses) <= 80, f"longest pauses around six saves: {pauses}"
    print(f"\n    longest frame delay around each of six saves: {pauses} ms")


# ---- Firmware upload: files that must be refused --------------------------------------


def still_running(device, before):
    """True if the board carried on without restarting."""
    after = device.json("/api/system")
    return after["uptimeS"] >= before["uptimeS"] and after["firmware"] == before["firmware"]


def test_upload_of_something_that_is_not_firmware_is_refused(device):
    before = device.json("/api/system")
    # A JPEG starts with FF D8; random bytes follow.
    response = upload(device, b"\xff\xd8" + os.urandom(40_000), "holiday-photo.jpg")
    assert response.status_code == 400
    assert response.json()["error"].startswith("The file is not ESP32 firmware")
    time.sleep(2.5)
    assert still_running(device, before)


def test_upload_of_a_tiny_file_is_refused(device):
    response = upload(device, b"\xe9too-short")
    assert response.status_code == 400
    assert "too small" in response.json()["error"]


def test_upload_with_no_file_is_refused(device):
    response = device.post("/api/update", timeout=UPLOAD_TIMEOUT)
    assert response.status_code == 400
    assert response.json()["error"] == "No firmware file was received"


def test_upload_of_the_factory_image_is_refused(device, request):
    """firmware.factory.bin is for flashing a blank board over USB; it is a whole flash
    image, not the application alone."""
    before = device.json("/api/system")
    response = upload(device, built_file(request, "firmware.factory.bin"), "firmware.factory.bin")
    assert response.status_code == 400
    assert "Upload the firmware.bin" in response.json()["error"]
    time.sleep(2.5)
    assert still_running(device, before)


def test_upload_of_the_bootloader_is_refused(device, request):
    """bootloader.bin is ESP32 code for the right chip, but it is not the application."""
    before = device.json("/api/system")
    response = upload(device, built_file(request, "bootloader.bin"), "bootloader.bin")
    assert response.status_code == 400
    assert "not the application" in response.json()["error"]
    time.sleep(2.5)
    assert still_running(device, before)


def test_upload_cut_short_leaves_the_running_firmware_alone(device, request):
    """Half a real image passes the first look but fails the final check."""
    before = device.json("/api/system")
    image = built_file(request, "firmware.bin")
    response = upload(device, image[: len(image) // 2])
    assert response.status_code == 400
    assert response.json()["error"].startswith("The update failed")
    time.sleep(2.5)
    assert still_running(device, before)
    assert device.state()["pattern"], "the board still answers normally"


def test_upload_from_someone_not_signed_in_writes_nothing(device, anonymous, request):
    before = device.json("/api/system")
    image = built_file(request, "firmware.bin")
    response = upload(anonymous, image)
    assert response.status_code == 401
    time.sleep(2.5)
    assert still_running(device, before)


# ---- Restart --------------------------------------------------------------------------


def test_restart_brings_the_board_back_with_its_settings(device):
    blend = {"cometMs": 4000, "flashMs": 3000, "crossfadeMs": 500, "randomDirection": False}
    device.set({"brightness": 37, "pattern": "blend", "blend": blend})
    time.sleep(2.5)  # settings are saved two quiet seconds after the last change

    response = device.post("/api/restart")
    assert response.status_code == 200
    assert response.json() == {"restarting": True}

    assert device.wait_until_gone(), "the board never went away"
    took = device.wait_until_back()
    assert device.get("/api/state").status_code == 401, "sessions do not survive a restart"

    device.login()
    info = device.json("/api/system")
    assert info["uptimeS"] < 40
    assert info["resetReason"] == "restart requested"
    state = device.state()
    assert state["brightness"] == 37 and state["pattern"] == "blend", "LED settings must survive a restart"
    assert state["blend"] == blend, "combined-pattern settings must survive a restart"
    print(f"\n    back {took:.0f} s after the restart request")


# ---- Real firmware upload (only with --firmware) ---------------------------------------


def test_firmware_upload_installs_and_restarts(device, request):
    path = request.config.getoption("--firmware")
    if not path:
        pytest.skip("give --firmware PATH to upload a real firmware.bin")
    with open(path, "rb") as handle:
        image = handle.read()

    device.set({"brightness": 42})
    time.sleep(2.5)
    wifi_before = device.json("/api/wifi")["station"]

    started = time.time()
    response = upload(device, image)
    assert response.status_code == 200, response.text
    assert response.json() == {"ok": True, "restarting": True}
    upload_seconds = time.time() - started

    assert device.wait_until_gone(), "the board never restarted"
    device.wait_until_back()
    device.login()

    info = device.json("/api/system")
    assert info["uptimeS"] < 40
    assert info["flash"]["firmwareBytes"] > 0
    assert device.state()["brightness"] == 42, "LED settings must survive an update"
    wifi_after = device.json("/api/wifi")["station"]
    assert wifi_after["ssid"] == wifi_before["ssid"], "the saved network must survive an update"
    print(f"\n    {len(image)} bytes uploaded in {upload_seconds:.0f} s; now running {info['firmware']}")


# ---- Factory reset (only with --destructive) --------------------------------------------


def test_factory_reset_needs_the_current_password(device):
    response = device.post("/api/factory-reset", {"currentPassword": "not-the-password"})
    assert response.status_code == 403
    assert device.post("/api/factory-reset", {}).status_code == 400
    assert device.get("/api/state").status_code == 200, "nothing was reset"


@pytest.mark.destructive
def test_factory_reset_restores_every_default(device):
    """Leaves the board with the default sign-in. On a development build it rejoins the
    network from platformio.local.ini; on a release build it is then only on its own
    access point and this test cannot see it come back."""
    info = device.json("/api/system")
    if not info["firmware"]["developmentBuild"] and device.host != "192.168.4.1":
        pytest.skip("after the reset a release build would not be reachable from this network")

    device.set({"brightness": 11, "pattern": "flash"})
    time.sleep(2.5)

    response = device.post("/api/factory-reset", {"currentPassword": device.password})
    assert response.status_code == 200
    assert response.json() == {"restarting": True}

    assert device.wait_until_gone(20), "the board never restarted"
    device.wait_until_back(90)

    fresh = device.anonymous()
    assert fresh.try_login("admin", "assimilate").status_code == 200, "default sign-in must work after a reset"
    session = fresh.json("/api/session")
    assert session["username"] == "admin" and session["defaultPassword"] is True

    state = fresh.state()
    for name, value in expected_defaults().items():
        assert state[name] == value, f"{name} must be back to its default"

    wifi = fresh.json("/api/wifi")
    assert wifi["accessPoint"]["ssid"] == "AssimilationEye"
    assert wifi["accessPoint"]["defaultPassword"] is True
    assert wifi["station"]["addressing"] == "dhcp"
    fresh.logout()

    device.username, device.password = "admin", "assimilate"
    device.login()
