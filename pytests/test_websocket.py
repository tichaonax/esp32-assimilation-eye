"""WS /ws: live two-way state."""
import time


def test_state_is_sent_on_connect(device, defaults):
    with device.socket() as socket:
        assert socket.receive_state() == defaults


def test_set_applies_and_answers_with_the_new_state(device):
    with device.socket() as socket:
        socket.receive_state()
        socket.send({"type": "set", "values": {"brightness": 15, "comet": {"tailLength": 1}}})

        state = socket.receive_state()
        assert state["brightness"] == 15
        assert state["comet"]["tailLength"] == 1
    assert device.state()["brightness"] == 15


def test_change_reaches_every_open_connection(device):
    with device.socket() as first, device.socket() as second:
        first.receive_state()
        second.receive_state()

        first.send({"type": "set", "values": {"pattern": "flash"}})

        assert first.receive_state()["pattern"] == "flash"
        assert second.receive_state()["pattern"] == "flash"


def test_change_made_over_http_is_pushed_to_the_socket(device):
    with device.socket() as socket:
        socket.receive_state()
        device.set({"brightness": 77})
        assert socket.receive_state()["brightness"] == 77


def test_values_are_clamped_like_the_http_route(device):
    with device.socket() as socket:
        socket.receive_state()
        socket.send({"type": "set", "values": {"brightness": 900, "comet": {"tailLength": -4}}})
        state = socket.receive_state()
        assert state["brightness"] == 100
        assert state["comet"]["tailLength"] == 0


def test_bad_value_gets_an_error_and_changes_nothing(device, defaults):
    with device.socket() as socket, device.socket() as other:
        socket.receive_state()
        other.receive_state()

        socket.send({"type": "set", "values": {"comet": {"tailLength": "long"}}})

        message = socket.receive()
        assert message == {"type": "error", "error": "comet.tailLength must be a number"}
        assert other.is_quiet(), "an error must only go to the sender"
    assert device.state() == defaults


def test_unknown_pattern_gets_an_error(device, defaults):
    with device.socket() as socket:
        socket.receive_state()
        socket.send({"type": "set", "values": {"pattern": "sparkle"}})
        assert socket.receive()["type"] == "error"
    assert device.state() == defaults


def test_set_without_values_gets_an_error(device):
    with device.socket() as socket:
        socket.receive_state()
        socket.send({"type": "set"})
        assert socket.receive()["type"] == "error"


def test_get_returns_the_state_to_the_sender_only(device, defaults):
    with device.socket() as socket, device.socket() as other:
        socket.receive_state()
        other.receive_state()
        socket.send({"type": "get"})
        assert socket.receive_state() == defaults
        assert other.is_quiet()


def test_reset_restores_the_defaults_for_everyone(device, defaults):
    device.set({"brightness": 5, "pattern": "flash"})
    with device.socket() as socket, device.socket() as other:
        socket.receive_state()
        other.receive_state()
        socket.send({"type": "reset"})
        assert socket.receive_state() == defaults
        assert other.receive_state() == defaults


def test_unknown_type_and_bad_json_get_errors_and_the_socket_stays_open(device):
    with device.socket() as socket:
        socket.receive_state()

        socket.send({"type": "dance"})
        assert socket.receive()["type"] == "error"

        socket.send_text("{not json")
        assert socket.receive()["type"] == "error"

        socket.send({"type": "set", "values": {"brightness": 20}})
        assert socket.receive_state()["brightness"] == 20


def test_slider_drag_is_followed_without_falling_behind(device):
    """Fifty changes sent as fast as a finger moves a slider."""
    with device.socket() as socket:
        socket.receive_state()
        start = time.perf_counter()
        for value in range(1, 51):
            socket.send({"type": "set", "values": {"brightness": value}})
            time.sleep(0.02)

        seen = 0
        while True:
            state = socket.receive_state()
            seen += 1
            if state["brightness"] == 50:
                break
        elapsed = time.perf_counter() - start

    # Updates may be dropped for a slow reader, but the last one must arrive
    # and the board must not lag far behind the finger.
    assert seen >= 1
    assert elapsed < 4.0, f"took {elapsed:.1f}s to settle"
    assert device.state()["brightness"] == 50


def test_reconnecting_many_times_does_not_exhaust_the_board(device, defaults):
    for _ in range(15):
        with device.socket() as socket:
            assert socket.receive_state() == defaults
