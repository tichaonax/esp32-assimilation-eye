"""GET /api/state and POST /api/reset."""
import statistics
import time


def test_state_reports_the_default_settings(device):
    state = device.state()

    assert state["running"] is True
    assert state["pattern"] == "comet"
    assert state["brightness"] == 60
    assert state["comet"] == {
        "direction": "clockwise",
        "stepIntervalMs": 90,
        "headBrightness": 100,
        "tailLength": 4,
        "tailFalloff": 50,
    }
    assert state["flash"] == {
        "intervalMs": 1000,
        "durationMs": 200,
        "minBrightness": 0,
        "maxBrightness": 100,
        "rampUpMs": 0,
        "rampDownMs": 0,
    }
    assert state["blend"] == {
        "cometMs": 8000,
        "flashMs": 6000,
        "crossfadeMs": 1500,
        "randomDirection": True,
    }
    assert state["onboard"] == {
        "mode": "blinkOnPatternChange",
        "brightness": 50,
        "blinkCount": 2,
        "blinkMs": 120,
    }


def test_state_lists_the_available_patterns(device):
    patterns = device.state()["patterns"]
    assert [p["name"] for p in patterns] == ["comet", "flash", "blend"]
    assert [p["label"] for p in patterns] == ["Rotating comet", "Synchronized flash", "Comet and flash"]


def test_state_says_whether_the_onboard_led_is_available(device):
    assert isinstance(device.state()["onboardAvailable"], bool)


def test_state_is_json_and_not_cached(device):
    response = device.get("/api/state")
    assert response.headers["Content-Type"].startswith("application/json")
    assert response.headers["Cache-Control"] == "no-store"


def test_reset_restores_the_defaults(device, defaults):
    device.set(
        {
            "brightness": 5,
            "pattern": "blend",
            "comet": {"tailLength": 1},
            "blend": {"cometMs": 2000, "randomDirection": False},
            "onboard": {"mode": "on"},
        }
    )
    assert device.state() != defaults

    assert device.reset() == defaults
    assert device.state() == defaults


def test_unknown_path_is_a_json_404(device):
    response = device.get("/api/nothing-here")
    assert response.status_code == 404
    assert "error" in response.json()


def test_wrong_method_is_refused(device):
    assert device.get("/api/config").status_code in (404, 405)
    assert device.post("/api/state").status_code in (404, 405)


def test_interface_page_is_served_compressed(device):
    response = device.get("/")
    assert response.status_code == 200
    assert response.headers["Content-Type"].startswith("text/html")
    assert response.headers["Content-Encoding"] == "gzip"
    # requests has already decompressed it.
    assert "<title>Assimilation Eye</title>" in response.text
    assert 'id="root"' in response.text


def test_interface_page_is_one_self_contained_file(device):
    page = device.get("/").text
    assert "/assets/" not in page, "the page must not need any other file from the board"
    assert '<script type="module">' in page
    assert "<style>" in page


def test_interface_page_is_revalidated_not_refetched(device):
    first = device.get("/")
    etag = first.headers["ETag"]
    assert first.headers["Cache-Control"] == "no-cache"

    again = device.http.get(device.base + "/", headers={"If-None-Match": etag}, timeout=5)
    assert again.status_code == 304
    assert again.content == b""
    assert again.headers["ETag"] == etag

    changed = device.http.get(device.base + "/", headers={"If-None-Match": '"something-else"'}, timeout=5)
    assert changed.status_code == 200


def test_index_html_is_the_same_page(device):
    assert device.get("/index.html").text == device.get("/").text


def test_state_answers_promptly_while_the_animation_runs(device):
    """The interface must stay responsive with LEDs animating."""
    timings = []
    for _ in range(20):
        start = time.perf_counter()
        device.state()
        timings.append(time.perf_counter() - start)
    assert statistics.median(timings) < 0.5, timings
