"""Sign-in: sessions, protected routes, lockout and changing the sign-in details."""
import time

import pytest
from websockets.exceptions import ConnectionClosed, InvalidStatus

TEMP_USER = "locutus"
TEMP_PASSWORD = "we-are-the-borg-7of9"


@pytest.fixture(autouse=True)
def clear_failed_attempts(device):
    """Wrong-password tests must not add up to a lockout for the tests after them."""
    yield
    extra = device.anonymous()
    extra.login()
    extra.logout()


# ---- Session ------------------------------------------------------------------


def test_session_says_not_signed_in_without_a_cookie(anonymous):
    body = anonymous.json("/api/session")
    assert body == {"authenticated": False}, "nothing about the account may be told to a stranger"


def test_session_says_who_is_signed_in(device):
    body = device.json("/api/session")
    assert body["authenticated"] is True
    assert body["username"] == device.username
    assert isinstance(body["defaultPassword"], bool)


def test_default_password_is_flagged(device):
    if device.password != "assimilate":
        pytest.skip("the board is not using the default password")
    assert device.json("/api/session")["defaultPassword"] is True


def test_interface_page_needs_no_sign_in(anonymous):
    response = anonymous.get("/")
    assert response.status_code == 200
    assert "<title>Assimilation Eye</title>" in response.text


# ---- Signing in -----------------------------------------------------------------


def test_sign_in_sets_a_protected_cookie(anonymous):
    response = anonymous.try_login()
    assert response.status_code == 200
    assert response.json()["authenticated"] is True

    cookie = response.headers["Set-Cookie"]
    assert cookie.startswith("eye_session=")
    token = cookie.split(";")[0].split("=", 1)[1]
    assert len(token) == 64 and all(c in "0123456789abcdef" for c in token)
    assert "HttpOnly" in cookie, "page scripts must not be able to read the session"
    assert "SameSite=Strict" in cookie, "other sites must not be able to use the session"
    assert "Path=/" in cookie
    assert response.headers["Cache-Control"] == "no-store"

    assert anonymous.get("/api/state").status_code == 200
    anonymous.logout()


def test_each_sign_in_gets_its_own_session(device, second):
    assert second.cookie != device.cookie


@pytest.mark.parametrize(
    "username,password",
    [("admin", "wrong-password"), ("nobody", "assimilate"), ("", ""), ("admin", ""), ("ADMIN", "assimilate")],
)
def test_wrong_details_are_refused(device, anonymous, username, password):
    if (username, password) == (device.username, device.password):
        pytest.skip("these happen to be the real details")
    response = anonymous.try_login(username, password)
    assert response.status_code == 401
    assert response.json() == {"error": "Wrong user name or password"}
    assert "Set-Cookie" not in response.headers
    assert anonymous.get("/api/state").status_code == 401


@pytest.mark.parametrize("payload", [{}, {"username": "admin"}, {"password": "assimilate"}, {"username": 1, "password": 2}])
def test_sign_in_needs_both_fields_as_text(anonymous, payload):
    assert anonymous.post("/api/login", payload).status_code == 400


def test_sign_in_with_a_very_long_password_is_simply_wrong(anonymous):
    response = anonymous.try_login("admin", "x" * 1500)
    assert response.status_code == 401


# ---- Protected routes -----------------------------------------------------------

PROTECTED = [
    ("GET", "/api/state"),
    ("POST", "/api/config"),
    ("POST", "/api/reset"),
    ("POST", "/api/logout"),
    ("POST", "/api/credentials"),
    ("GET", "/api/wifi"),
    ("POST", "/api/wifi"),
    ("DELETE", "/api/wifi"),
    ("GET", "/api/wifi/scan"),
    ("POST", "/api/wifi/ap"),
    ("GET", "/api/system"),
    ("POST", "/api/restart"),
    ("POST", "/api/factory-reset"),
    ("POST", "/api/update"),
]


@pytest.mark.parametrize("method,path", PROTECTED, ids=[f"{m} {p}" for m, p in PROTECTED])
def test_route_refuses_a_caller_who_is_not_signed_in(anonymous, method, path):
    response = anonymous.http.request(method, anonymous.base + path, json={"brightness": 1}, timeout=(2, 8))
    assert response.status_code == 401, f"{method} {path} answered {response.status_code}"
    assert response.json() == {"error": "Sign in first"}


@pytest.mark.parametrize("cookie", ["", "nonsense", "0" * 64, "f" * 63, "f" * 65, "'; DROP TABLE sessions;--"])
def test_made_up_session_cookie_is_refused(anonymous, cookie):
    response = anonymous.http.get(
        anonymous.base + "/api/state", headers={"Cookie": f"eye_session={cookie}"}, timeout=(2, 8)
    )
    assert response.status_code == 401


def test_refused_request_changes_nothing(device, anonymous, defaults):
    anonymous.post("/api/config", {"brightness": 1})
    anonymous.post("/api/reset")
    assert device.state() == defaults


def test_websocket_refuses_a_caller_who_is_not_signed_in(anonymous):
    with pytest.raises(InvalidStatus) as refused:
        anonymous.socket()
    assert refused.value.response.status_code == 401


def test_websocket_refuses_a_made_up_cookie(anonymous):
    anonymous.http.cookies.set("eye_session", "a" * 64)
    with pytest.raises(InvalidStatus):
        anonymous.socket()


# ---- Signing out ----------------------------------------------------------------


def test_sign_out_ends_the_session(device):
    other = device.anonymous()
    other.login()
    token = other.cookie
    assert other.get("/api/state").status_code == 200

    response = other.logout()
    assert response.status_code == 200
    assert response.json() == {"authenticated": False}
    assert "Max-Age=0" in response.headers["Set-Cookie"]

    # Even with the old token put back by hand, the session is gone.
    replay = device.anonymous()
    replay.http.cookies.set("eye_session", token)
    assert replay.get("/api/state").status_code == 401
    assert device.get("/api/state").status_code == 200, "signing out must not end anyone else's session"


def test_sign_out_closes_live_connections(device):
    other = device.anonymous()
    other.login()
    socket = other.socket()
    socket.receive_state()
    other.logout()
    with pytest.raises(ConnectionClosed):
        for _ in range(5):
            socket.connection.recv(timeout=3)
    # A browser that is still signed in simply reconnects.
    with device.socket() as mine:
        mine.receive_state()


# ---- Changing the sign-in details -------------------------------------------------

BAD_CHANGES = [
    ({"username": "ab", "password": "long-enough-password"}, "user name must be 3 to 32 characters"),
    ({"username": "u" * 33, "password": "long-enough-password"}, "user name must be 3 to 32 characters"),
    ({"username": " admin", "password": "long-enough-password"}, "must not start or end with a space"),
    ({"username": "admin", "password": "short"}, "password must be 8 to 64 characters"),
    ({"username": "admin", "password": "p" * 65}, "password must be 8 to 64 characters"),
]


@pytest.mark.parametrize("change,message", BAD_CHANGES, ids=[m for _, m in BAD_CHANGES])
def test_new_details_must_follow_the_rules(device, change, message):
    response = device.post("/api/credentials", {"currentPassword": device.password, **change})
    assert response.status_code == 400
    assert message in response.json()["error"]
    assert device.json("/api/session")["username"] == device.username


@pytest.mark.parametrize("missing", ["currentPassword", "username", "password"])
def test_change_needs_every_field(device, missing):
    payload = {"currentPassword": device.password, "username": TEMP_USER, "password": TEMP_PASSWORD}
    del payload[missing]
    assert device.post("/api/credentials", payload).status_code == 400


def test_change_needs_the_current_password(device):
    response = device.post(
        "/api/credentials", {"currentPassword": "not-the-password", "username": TEMP_USER, "password": TEMP_PASSWORD}
    )
    assert response.status_code == 403
    assert response.json() == {"error": "The current password is wrong"}
    assert device.json("/api/session")["username"] == device.username


def test_details_can_be_changed_and_changed_back(device):
    bystander = device.anonymous()
    bystander.login()
    changer = device.anonymous()
    changer.login()
    try:
        response = changer.post(
            "/api/credentials",
            {"currentPassword": device.password, "username": TEMP_USER, "password": TEMP_PASSWORD},
        )
        assert response.status_code == 200, response.text
        assert response.json() == {"authenticated": True, "username": TEMP_USER, "defaultPassword": False}

        assert changer.get("/api/state").status_code == 200, "the session that made the change stays signed in"
        assert bystander.get("/api/state").status_code == 401, "every other session is ended"
        assert device.get("/api/state").status_code == 401

        assert device.anonymous().try_login().status_code == 401, "the old details no longer work"
        fresh = device.anonymous()
        assert fresh.try_login(TEMP_USER, TEMP_PASSWORD).status_code == 200
        fresh.logout()
    finally:
        restore = changer.post(
            "/api/credentials",
            {"currentPassword": TEMP_PASSWORD, "username": device.username, "password": device.password},
        )
        device.login()
    assert restore.status_code == 200, f"COULD NOT RESTORE THE SIGN-IN DETAILS: {restore.text}"
    assert device.json("/api/session")["username"] == device.username
    changer.logout()


# ---- Lockout ----------------------------------------------------------------------


def test_five_wrong_attempts_lock_sign_in_for_thirty_seconds(device):
    attacker = device.anonymous()
    for attempt in range(5):
        response = attacker.try_login(device.username, f"guess-number-{attempt}")
        assert response.status_code == 401, f"attempt {attempt + 1} answered {response.status_code}"

    locked = attacker.try_login(device.username, "guess-number-6")
    assert locked.status_code == 429
    body = locked.json()
    assert 1 <= body["retryAfterS"] <= 30
    assert str(body["retryAfterS"]) in body["error"]
    assert locked.headers["Retry-After"] == str(body["retryAfterS"])

    # During the lockout even the right password is refused...
    assert attacker.try_login().status_code == 429
    # ...but someone already signed in is not disturbed.
    assert device.get("/api/state").status_code == 200

    time.sleep(body["retryAfterS"] + 1.5)
    owner = device.anonymous()
    assert owner.try_login().status_code == 200, "sign-in must work again once the lockout has passed"
    owner.logout()
