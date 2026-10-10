"""Capture the LED settings on a board as the defaults for future builds.

Tune the ring from the browser until it looks right, then run:

    python scripts/save_led_defaults.py --device 192.168.1.50

It signs in, reads the board's current LED settings and writes them to
led-defaults.json next to platformio.ini. The next build is compiled with
them as its defaults. Commit the file to keep them.

    --device ADDRESS     or the EYE_DEVICE variable; default assimilation-eye.local
    --user NAME          or EYE_USER; default admin
    --password PASSWORD  or EYE_PASSWORD; asked for if neither is given
    --show               print what the board has and write nothing

To go back to the built-in defaults, delete led-defaults.json.

Needs only Python 3.9 or newer; nothing to install.
"""
import argparse
import getpass
import http.cookiejar
import json
import os
import sys
import urllib.error
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import led_defaults  # noqa: E402

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The order the file is written in: the same as the interface.
GROUPS = ("comet", "flash", "blend", "onboard")


class Board:
    def __init__(self, host):
        self.base = "http://" + host.removeprefix("http://").rstrip("/")
        self._opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))

    def call(self, path, body=None):
        """Returns (status, parsed JSON). Raises OSError if the board does not answer."""
        data = None if body is None else json.dumps(body).encode()
        headers = {} if body is None else {"Content-Type": "application/json"}
        last = None
        # The board now and then ignores a new connection; a second attempt
        # on a fresh one gets through.
        for _ in range(4):
            request = urllib.request.Request(self.base + path, data=data, headers=headers)
            try:
                with self._opener.open(request, timeout=4) as reply:
                    return reply.status, json.loads(reply.read().decode())
            except urllib.error.HTTPError as error:
                text = error.read().decode(errors="replace")
                try:
                    return error.code, json.loads(text)
                except json.JSONDecodeError:
                    return error.code, {"error": text.strip()}
            except OSError as error:
                last = error
        raise OSError(last)


def settings_from_state(state):
    """The LED settings out of GET /api/state, leaving out what is not a setting."""
    settings = {name: state[name] for name in led_defaults.SETTINGS[None] if name in state}
    for group in GROUPS:
        if group in state:
            settings[group] = {name: state[group][name] for name in led_defaults.SETTINGS[group] if name in state[group]}
    return settings


def flatten(settings):
    flat = {}
    for key, value in (settings or {}).items():
        if isinstance(value, dict):
            for name, inner in value.items():
                flat[f"{key}.{name}"] = inner
        else:
            flat[key] = value
    return flat


def main():
    parser = argparse.ArgumentParser(description="Save a board's LED settings as the defaults for future builds.")
    parser.add_argument("--device", default=os.environ.get("EYE_DEVICE", "assimilation-eye.local"))
    parser.add_argument("--user", default=os.environ.get("EYE_USER", "admin"))
    parser.add_argument("--password", default=os.environ.get("EYE_PASSWORD"))
    parser.add_argument("--show", action="store_true", help="print the board's settings and write nothing")
    args = parser.parse_args()

    password = args.password
    if password is None:
        password = getpass.getpass(f"Password for {args.user} on {args.device}: ")

    board = Board(args.device)
    try:
        status, reply = board.call("/api/login", {"username": args.user, "password": password})
        if status != 200:
            sys.exit(f"Could not sign in to {board.base}: {reply.get('error', status)}")
        status, state = board.call("/api/state")
        board.call("/api/logout", {})
    except OSError as error:
        sys.exit(f"Could not reach the board at {board.base}: {error}\nGive its address with --device.")
    if status != 200:
        sys.exit(f"The board did not give its settings: {state.get('error', status)}")

    settings = settings_from_state(state)
    try:
        led_defaults.check(settings, led_defaults.read_limits(PROJECT_DIR), led_defaults.read_pattern_names(PROJECT_DIR))
    except led_defaults.DefaultsError as error:
        sys.exit(
            f"The board's settings do not fit this source tree: {error}\n"
            "The board is probably running a different firmware version. Upload this one and try again."
        )

    text = json.dumps(settings, indent=2) + "\n"
    if args.show:
        print(text, end="")
        return

    path = os.path.join(PROJECT_DIR, led_defaults.FILE_NAME)
    try:
        with open(path, encoding="utf-8-sig") as handle:
            before = flatten(json.load(handle))
        existed = True
    except (OSError, json.JSONDecodeError):
        before, existed = {}, False

    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)

    after = flatten(settings)
    print(f"Wrote {len(after)} settings from {board.base} to {led_defaults.FILE_NAME}.")
    if existed:
        changed = [name for name in after if before.get(name) != after[name]]
        if changed:
            for name in changed:
                was = json.dumps(before[name]) if name in before else "not set"
                print(f"  {name}: {was} -> {json.dumps(after[name])}")
        else:
            print("  Nothing changed from the file that was there.")
    if settings.get("running") is False:
        print("Note: the ring was stopped, so builds with this file start with the ring stopped.")
    print("The next build uses these as its defaults. Commit the file to keep them.")


if __name__ == "__main__":
    main()
