"""API tests for the Assimilation Eye, run from the PC against a live board.

Set-up, once:

    py -3 -m venv pytests/.venv
    pytests/.venv/Scripts/python -m pip install -r pytests/requirements.txt

Run, giving the board's address (printed on its serial console at start-up):

    pytests/.venv/Scripts/python -m pytest pytests --device 192.168.1.50

or set the EYE_DEVICE environment variable instead of passing --device.

The PC must be able to reach the board: either the PC is on the prop's own
access point (address 192.168.4.1), or the board has joined the PC's network
(see platformio.local.ini.example).

These tests change the LED settings and finish by restoring the defaults, so
whatever was saved on the board before the run is replaced by the defaults.
"""
import json
import os

import pytest
import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry
from websockets.sync.client import connect

TIMEOUT_S = 5

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
    parser.addoption(
        "--device",
        default=os.environ.get("EYE_DEVICE"),
        help="Address of the board, e.g. 192.168.4.1 (or set EYE_DEVICE)",
    )


class Device:
    """Thin client for the board's HTTP and WebSocket API."""

    def __init__(self, host):
        self.host = host
        self.base = f"http://{host}"
        self.ws_url = f"ws://{host}/ws"
        self.http = requests.Session()
        # Retry only a connection that was never established. A request that
        # was sent and went unanswered is a real failure and is not retried.
        retry = Retry(total=None, connect=CONNECT_ATTEMPTS - 1, read=0, status=0, other=0, backoff_factor=0)
        self.http.mount("http://", HTTPAdapter(max_retries=retry))
        self.connect_retries = 0

    def get(self, path):
        return self.http.get(self.base + path, timeout=(CONNECT_TIMEOUT_S, TIMEOUT_S))

    def post(self, path, payload=None, **kwargs):
        if payload is not None:
            kwargs["json"] = payload
        return self.http.post(self.base + path, timeout=(CONNECT_TIMEOUT_S, TIMEOUT_S), **kwargs)

    def state(self):
        response = self.get("/api/state")
        assert response.status_code == 200, response.text
        return response.json()

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
        for attempt in range(CONNECT_ATTEMPTS):
            try:
                return Socket(connect(self.ws_url, open_timeout=CONNECT_TIMEOUT_S))
            except TimeoutError:
                if attempt == CONNECT_ATTEMPTS - 1:
                    raise
                self.connect_retries += 1


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

    def close(self):
        self.connection.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


@pytest.fixture(scope="session")
def device(request):
    host = request.config.getoption("--device")
    if not host:
        pytest.exit("Give the board's address with --device or the EYE_DEVICE variable.", returncode=2)
    host = host.removeprefix("http://").rstrip("/")
    board = Device(host)
    try:
        board.get("/api/state")
    except requests.RequestException as error:
        pytest.exit(f"Cannot reach the board at {board.base}: {error}", returncode=2)
    return board


@pytest.fixture(autouse=True)
def default_settings(device):
    """Every test starts from, and leaves behind, the default LED settings."""
    device.reset()
    yield
    device.reset()


@pytest.fixture
def defaults(device):
    """The default state, as the board reports it."""
    return device.state()
