"""API tests for the Assimilation Eye, run from the PC against a live board.

Set-up, once:

    py -3 -m venv pytests/.venv
    pytests/.venv/Scripts/python -m pip install -r pytests/requirements.txt

Run, giving the board's address (printed on its serial console at start-up):

    pytests/.venv/Scripts/python -m pytest pytests --device 192.168.1.50

Options (each also has an environment variable):

    --device ADDRESS     EYE_DEVICE     the board's address
    --user NAME          EYE_USER       sign-in user name (default: admin)
    --password WORD      EYE_PASSWORD   sign-in password (default: assimilate)
    --firmware FILE                     also upload this firmware.bin and wait for the restart
    --destructive                       also run tests that change network settings
                                        or factory-reset the board

The PC must be able to reach the board: either the PC is on the prop's own
access point (address 192.168.4.1), or the board has joined the PC's network.

What a normal run does to the board: changes LED settings and finishes by
restoring the defaults; signs in and out many times; triggers the 30-second
sign-in lockout once; changes the sign-in details and changes them back;
searches for Wi-Fi networks; and restarts the board once.
"""
import copy
import json
import os
import time

import pytest
import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry
from websockets.sync.client import connect

TIMEOUT_S = 8

# The board closes every HTTP connection after one reply and, like any TCP
# stack, remembers a closed connection for a while. Its table is small (16
# entries) and, unlike a desktop stack, it ignores a new connection that
# arrives from the same client port as one it still remembers. Windows does
# occasionally hand out a port it used a moment ago, so about one connection
# in a thousand gets no answer. Nothing has been sent at that point, so trying
# again on a new connection is always safe; the interface does the same.
CONNECT_TIMEOUT_S = 2
CONNECT_ATTEMPTS = 4


def pytest_addoption(parser):
    parser.addoption("--device", default=os.environ.get("EYE_DEVICE"), help="Address of the board, e.g. 192.168.4.1")
    parser.addoption("--user", default=os.environ.get("EYE_USER", "admin"), help="Sign-in user name")
    parser.addoption("--password", default=os.environ.get("EYE_PASSWORD", "assimilate"), help="Sign-in password")
    parser.addoption("--firmware", default=None, help="firmware.bin to upload in the update test")
    parser.addoption("--destructive", action="store_true", help="Run tests that change network settings or factory-reset")


def pytest_configure(config):
    config.addinivalue_line("markers", "destructive: changes network settings or factory-resets; needs --destructive")


def pytest_collection_modifyitems(config, items):
    if config.getoption("--destructive"):
        return
    skip = pytest.mark.skip(reason="needs --destructive")
    for item in items:
        if "destructive" in item.keywords:
            item.add_marker(skip)


class Device:
    """Thin client for the board's HTTP and WebSocket API, with its own cookies."""

    def __init__(self, host, username, password):
        self.host = host
        self.base = f"http://{host}"
        self.ws_url = f"ws://{host}/ws"
        self.username = username
        self.password = password
        self.http = requests.Session()
        # Retry only a connection that was never established. A request that
        # was sent and went unanswered is a real failure and is not retried.
        retry = Retry(total=None, connect=CONNECT_ATTEMPTS - 1, read=0, status=0, other=0, backoff_factor=0)
        self.http.mount("http://", HTTPAdapter(max_retries=retry))

    # ---- plain requests ----

    def get(self, path, **kwargs):
        return self.http.get(self.base + path, timeout=(CONNECT_TIMEOUT_S, TIMEOUT_S), **kwargs)

    def post(self, path, payload=None, **kwargs):
        if payload is not None:
            kwargs["json"] = payload
        kwargs.setdefault("timeout", (CONNECT_TIMEOUT_S, TIMEOUT_S))
        return self.http.post(self.base + path, **kwargs)

    def delete(self, path):
        return self.http.delete(self.base + path, timeout=(CONNECT_TIMEOUT_S, TIMEOUT_S))

    def json(self, path):
        response = self.get(path)
        assert response.status_code == 200, f"{path}: {response.status_code} {response.text}"
        return response.json()

    # ---- sign-in ----

    def anonymous(self):
        """A second client for the same board that has not signed in."""
        return Device(self.host, self.username, self.password)

    def try_login(self, username=None, password=None):
        return self.post(
            "/api/login",
            {"username": self.username if username is None else username,
             "password": self.password if password is None else password},
        )

    def login(self):
        """Signs in, waiting out a lockout if one is in force."""
        for _ in range(3):
            response = self.try_login()
            if response.status_code == 429:
                time.sleep(response.json()["retryAfterS"] + 1)
                continue
            assert response.status_code == 200, f"sign-in failed: {response.status_code} {response.text}"
            return response.json()
        raise AssertionError("still locked out after waiting")

    def logout(self):
        return self.post("/api/logout")

    @property
    def cookie(self):
        return self.http.cookies.get("eye_session")

    # ---- LED settings ----

    def state(self):
        return self.json("/api/state")

    def set(self, values):
        """POST partial settings and return the resulting state."""
        response = self.post("/api/config", values)
        assert response.status_code == 200, response.text
        return response.json()

    def reset(self):
        response = self.post("/api/reset")
        assert response.status_code == 200, response.text
        return response.json()

    def socket(self):
        headers = {"Cookie": f"eye_session={self.cookie}"} if self.cookie else {}
        for attempt in range(CONNECT_ATTEMPTS):
            try:
                return Socket(connect(self.ws_url, open_timeout=CONNECT_TIMEOUT_S, additional_headers=headers))
            except TimeoutError:
                if attempt == CONNECT_ATTEMPTS - 1:
                    raise

    # ---- restarts ----

    def wait_until_gone(self, seconds=15):
        """Waits for the board to stop answering. True if it did."""
        end = time.time() + seconds
        while time.time() < end:
            try:
                requests.get(self.base + "/api/session", timeout=(1, 1))
            except requests.RequestException:
                return True
            time.sleep(0.3)
        return False

    def wait_until_back(self, seconds=60):
        """Waits for the board to answer again. Returns how long it took."""
        start = time.time()
        while time.time() - start < seconds:
            try:
                if requests.get(self.base + "/api/session", timeout=(1.5, 2)).status_code == 200:
                    return time.time() - start
            except requests.RequestException:
                pass
            time.sleep(0.5)
        raise AssertionError(f"the board did not come back within {seconds} s")


class Socket:
    """A WebSocket connection that speaks JSON."""

    def __init__(self, connection):
        self.connection = connection

    def send(self, message):
        self.connection.send(json.dumps(message))

    def send_text(self, text):
        self.connection.send(text)

    def receive(self):
        return json.loads(self.connection.recv(timeout=TIMEOUT_S))

    def receive_state(self):
        message = self.receive()
        assert message["type"] == "state", message
        return message["state"]

    def is_quiet(self, seconds=0.5):
        """True if nothing arrives within the given time."""
        try:
            self.connection.recv(timeout=seconds)
        except TimeoutError:
            return True
        return False

    def drain(self):
        """Reads and discards whatever has arrived.

        This client stops reading from the network once 16 messages are
        waiting, and would then miss the board's reply to a close, so a test
        that sends a lot must keep reading.
        """
        while not self.is_quiet(0):
            pass

    def close(self):
        self.connection.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


@pytest.fixture(scope="session")
def device(request):
    """The board, signed in."""
    host = request.config.getoption("--device")
    if not host:
        pytest.exit("Give the board's address with --device or the EYE_DEVICE variable.", returncode=2)
    host = host.removeprefix("http://").rstrip("/")
    board = Device(host, request.config.getoption("--user"), request.config.getoption("--password"))
    try:
        board.get("/api/session")
    except requests.RequestException as error:
        pytest.exit(f"Cannot reach the board at {board.base}: {error}", returncode=2)
    try:
        board.login()
    except AssertionError as error:
        pytest.exit(f"Cannot sign in to the board ({error}). Give --user and --password.", returncode=2)
    yield board
    board.logout()


@pytest.fixture
def anonymous(device):
    """The same board, through a client that has not signed in."""
    return device.anonymous()


@pytest.fixture
def second(device):
    """A second signed-in client, signed out again afterwards."""
    other = device.anonymous()
    other.login()
    yield other
    other.logout()


@pytest.fixture(autouse=True)
def default_settings(device):
    """Every test starts from, and leaves behind, the default LED settings."""
    device.reset()
    yield
    device.reset()


# The built-in defaults in lib/eye_core/EyeConfig.cpp.
BUILT_IN_DEFAULTS = {
    "running": True,
    "pattern": "comet",
    "brightness": 60,
    "comet": {
        "direction": "clockwise",
        "stepIntervalMs": 90,
        "headBrightness": 100,
        "tailLength": 4,
        "tailFalloff": 50,
    },
    "flash": {
        "intervalMs": 1000,
        "durationMs": 200,
        "minBrightness": 0,
        "maxBrightness": 100,
        "rampUpMs": 0,
        "rampDownMs": 0,
    },
    "blend": {
        "cometMs": 8000,
        "flashMs": 6000,
        "crossfadeMs": 1500,
        "randomDirection": True,
    },
    "onboard": {
        "mode": "blinkOnPatternChange",
        "brightness": 50,
        "blinkCount": 2,
        "blinkMs": 120,
    },
}


def expected_defaults():
    """What a reset should give: the built-in table with this source tree's led-defaults.json on top.

    The board must have been built from this tree, as it must for every other test here.
    """
    expected = copy.deepcopy(BUILT_IN_DEFAULTS)
    custom = led_defaults_file()
    for key, value in (custom or {}).items():
        if isinstance(value, dict):
            expected[key].update(value)
        else:
            expected[key] = value
    return expected


def led_defaults_file():
    """The parsed led-defaults.json, or None if the source tree has none."""
    path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "led-defaults.json")
    if not os.path.isfile(path):
        return None
    with open(path, encoding="utf-8-sig") as handle:
        return json.load(handle)


@pytest.fixture
def defaults(device):
    """The default state, as the board reports it."""
    return device.state()
