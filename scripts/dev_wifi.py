"""PlatformIO pre-build script: optional development Wi-Fi.

If platformio.local.ini exists next to platformio.ini and has a [dev_wifi]
section with an ssid, the firmware is built to join that network as well as
running its own access point. This lets the PC reach the board for the API
tests without leaving its own Wi-Fi.

The file is git-ignored. Without it, nothing is added and the firmware only
runs its access point. Never ship a firmware built with it: the network name
and password are compiled into the image.
"""
import configparser
import os

Import("env")  # noqa: F821  (provided by PlatformIO)

local_ini = os.path.join(env.subst("$PROJECT_DIR"), "platformio.local.ini")  # noqa: F821

if os.path.isfile(local_ini):
    # interpolation=None: a password may contain a % sign.
    parser = configparser.ConfigParser(interpolation=None)
    parser.read(local_ini, encoding="utf-8")

    def value(name):
        text = parser.get("dev_wifi", name, fallback="").strip()
        # Quotes are not part of ini syntax; drop a matching pair if someone adds them.
        if len(text) >= 2 and text[0] == text[-1] and text[0] in "\"'":
            text = text[1:-1]
        return text

    ssid = value("ssid")
    password = value("password")
    if ssid:
        env.Append(  # noqa: F821
            CPPDEFINES=[
                ("DEV_WIFI_SSID", env.StringifyMacro(ssid)),  # noqa: F821
                ("DEV_WIFI_PASSWORD", env.StringifyMacro(password)),  # noqa: F821
            ]
        )
        # The network name and password are deliberately not printed.
        print("dev_wifi: DEVELOPMENT BUILD - joins the network from platformio.local.ini")
    else:
        print("dev_wifi: platformio.local.ini has no [dev_wifi] ssid - access point only")
