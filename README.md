# Assimilation Eye

Firmware and browser interface for a wearable prop: eight red LEDs in a ring around one eyeglass lens, driven by an ESP32 and controlled live from a phone.

- **Using the prop:** [docs/user-guide.md](docs/user-guide.md)
- **Building the circuit:** [docs/hardware-guide.md](docs/hardware-guide.md)
- **This file:** building, uploading, testing and changing the firmware

## What it does

- Three patterns: rotating comet, synchronized flash, and a combination that crossfades between the two
- Every pattern setting adjustable live from a browser, over a WebSocket, with no reflash
- Its own Wi-Fi access point, always on, and optionally joins a network with DHCP or a static address
- Sign-in, firmware update from the browser, and factory reset from the BOOT button
- Settings stored on the board and restored at power-on

Boards: DOIT ESP32 DEVKIT V1 (30-pin) and ESP32-DevKitC (38-pin). One build serves both.

## Layout

| Path | Contents |
|---|---|
| `platformio.ini` | Build configuration: the `esp32dev` firmware and the `native` PC test build |
| `include/HardwareConfig.h` | Every pin, and the PWM settings. The only file that knows the wiring |
| `include/Defaults.h` | Factory sign-in and access point details |
| `lib/eye_core/` | Hardware-independent C++: patterns, settings, validation, JSON, input rules. Unit-tested on the PC and on the board |
| `src/` | Device code: LED driver, animation task, Wi-Fi, sign-in, web API, update, factory reset |
| `ui/` | Browser interface: Vite, React 19, TypeScript. Built into one compressed file and embedded in the firmware |
| `scripts/` | PlatformIO build scripts, and `save_led_defaults.py` |
| `scripts/tests/` | Tests for the build scripts. No board needed |
| `test/test_core/` | C++ unit tests (Unity) |
| `pytests/` | API tests, run against a board over Wi-Fi |
| `led-defaults.json` | Optional. The LED settings a build starts with and resets to |
| `docs/` | User guide and hardware guide |

Generated and ignored by git: `.pio/`, `src/generated/`, `lib/eye_core/generated/`, `ui/node_modules/`, `ui/dist/`, `pytests/.venv/`, `platformio.local.ini`.

## What you need

| Tool | For | Notes |
|---|---|---|
| [PlatformIO Core](https://platformio.org/install/cli) | Everything | The VS Code PlatformIO extension includes it |
| Node `^20.19` or `>=22.12`, with npm | Building the interface | Needed for the first build and whenever `ui/` changes. A suitable version installed with nvm for Windows is found automatically |
| MinGW-w64 `g++` on `PATH` | PC unit tests only | For example WinLibs, installed through `winget` |
| Python 3.9 or newer | API tests and `save_led_defaults.py` | |
| CP210x USB driver | Talking to the board | Windows usually installs it by itself |

The first build downloads the pinned platform (pioarduino 55.03.312-1: Arduino core 3.3.12 on ESP-IDF 5.5) and its toolchains into `%USERPROFILE%\.pio-assimilation-eye`. This project keeps them apart from the default `~/.platformio` on purpose, so other PlatformIO projects on the PC are not affected. Allow several minutes.

### Running `pio` on Windows

Run `pio` from **PowerShell or cmd, not Git Bash**. The platform's tool installer refuses to run under MSYS shells.

If PowerShell says `pio` is not recognized, it is not on `PATH`. Either use the full path:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload
```

or define a shortcut for the session, which the rest of this file assumes:

```powershell
Set-Alias pio "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
```

## Build and upload

Connect the board by USB.

```powershell
pio run                 # build
pio run -t upload       # build and upload over USB
pio device monitor      # serial console, 115200 baud; Ctrl+C to leave
```

No serial port is configured: PlatformIO finds the board by its USB id, so the COM port may change freely. With two CP210x devices attached, add `--upload-port COM7`.

If the upload ends with "Failed to connect", hold the board's BOOT button as the upload starts and release it when writing begins.

After the upload the board restarts, runs its self-test (LED 1 to LED 8, then a fade) and prints its addresses on the console. Section 9 of the hardware guide shows a healthy start-up.

The first upload to a board must be over USB, because it also writes the partition table. After that, `firmware.bin` can be installed from the browser's System screen:

```
.pio\build\esp32dev\firmware.bin
```

### The build output, line by line

| Build log line | Meaning |
|---|---|
| `led_defaults: CUSTOM LED DEFAULTS - 22 settings from led-defaults.json` | The build takes its LED defaults from the file. Without the file it says the built-in defaults are used |
| `dev_wifi: DEVELOPMENT BUILD - …` | A `platformio.local.ini` is present. See below |
| `embed_ui: …` | Whether the interface was rebuilt or was unchanged |

## LED defaults for a build

The settings a board starts with when nothing is saved, and returns to on "Reset LED settings" or a factory reset, come from `led-defaults.json` next to `platformio.ini`. The file is optional and is committed with the source.

**To set them from a board you have tuned:**

1. Adjust the ring in the browser until it looks right.
2. Capture the board's settings into the file:

   ```powershell
   python scripts\save_led_defaults.py --device 192.168.4.1
   ```

   It asks for the sign-in password, writes all 22 settings and lists what changed. `--show` prints the board's settings without writing. `--device` defaults to `assimilation-eye.local`; `--user` and `--password` are available.
3. Build and upload. The build log confirms the file was used, and the System screen shows **LED defaults: Set for this build**.
4. Commit `led-defaults.json`.

**To edit by hand:** the file has the same shape as the `/api/state` reply, and may hold any subset. Anything left out keeps its built-in value.

```json
{
  "pattern": "blend",
  "brightness": 84,
  "comet": { "tailLength": 5 }
}
```

A setting that does not exist, a value of the wrong type or a value out of range **stops the build** with a message naming it, for example `comet.tailLength is 12; it must be from 0 to 7`. The ranges are read from `lib/eye_core/EyeConfig.h`, so they are never out of step with the firmware.

**To go back to the built-in defaults:** delete the file and rebuild. The built-in table is `defaultConfig()` in `lib/eye_core/EyeConfig.cpp`.

A board that already has settings saved keeps them across an upload. Use "Reset LED settings" to see the new defaults.

## Development Wi-Fi and release builds

To let the PC reach the board without leaving its own Wi-Fi, copy `platformio.local.ini.example` to `platformio.local.ini` and enter a 2.4 GHz network. The firmware then joins it at start-up as well as running its own access point.

That file is ignored by git. **A firmware built with it has the network name and password compiled in.** The build log, the serial console and the System screen all mark it as a development build.

**To make a firmware that can be handed to someone else:** delete or rename `platformio.local.ini`, rebuild, and check that the build log has no `DEVELOPMENT BUILD` line.

## Tests

Three sets. All were passing when this was written.

### Unit tests (C++, Unity)

```powershell
pio test -e native      # on the PC; needs g++ on PATH
pio test -e esp32dev    # on the board, over USB
```

121 tests of the patterns, settings validation, JSON, input rules, address rules, sign-in lockout and firmware-file checks. One or two are skipped by design, depending on whether `led-defaults.json` is present.

`pio test -e esp32dev` replaces the firmware on the board with the test runner. Run `pio run -t upload` afterwards.

### Build-script tests (Python, no board)

```powershell
pytests\.venv\Scripts\python -m pytest scripts\tests -q
```

### API tests (Python, against a board over Wi-Fi)

One-time setup:

```powershell
python -m venv pytests\.venv
pytests\.venv\Scripts\pip install -r pytests\requirements.txt
```

Run, giving the board's address:

```powershell
pytests\.venv\Scripts\python -m pytest pytests --device 192.168.4.1 -q
```

Options:

| Option | Default | Purpose |
|---|---|---|
| `--device` | `EYE_DEVICE` variable | Address of the board. Required |
| `--user`, `--password` | `admin`, `assimilate` | Sign-in details |
| `--destructive` | off | Also run the tests that change network settings and factory-reset the board |
| `--firmware PATH` | none | With `--destructive`, upload this `firmware.bin` as a real update |

The tests expect the board to be running a firmware built from this source tree, including its `led-defaults.json`, and they leave the LED settings at their defaults. The run takes three to four minutes because the sign-in lockout tests wait out real lockouts.

The factory-reset test is skipped on a release build reached over a joined network, since the board would forget that network.

## Working on the interface

```powershell
cd ui
npm ci
$env:EYE_DEVICE = "192.168.4.1"   # the board to talk to
npm run dev
```

The page is served from the PC with live reload, and its API calls are passed to the board. `npm run build` type-checks, lints and builds. There is no need to run it by hand for a firmware build: `scripts/embed_ui.py` does so when anything under `ui/` has changed.

## Adding a pattern

1. Write a class in `lib/eye_core/` that implements `Pattern` (`Pattern.h`): a name, a label, `reset()` and `render()`. `render()` receives the elapsed time and writes a brightness from 0 to 1 for each LED. Patterns advance by elapsed time and never read a clock, so they can be tested exactly and a speed change never causes a jump.
2. Put its settings in `EyeConfig` (`EyeConfig.h`) with their limits, defaults and clamping in `EyeConfig.cpp`.
3. Read and write them in `ConfigJson.cpp`, and add them to `SETTINGS` in `scripts/led_defaults.py`. A test fails if the two disagree.
4. Register the class in the `PatternRegistry` constructor. The pattern picker in the interface lists whatever is registered.
5. Add a card for its settings in `ui/src/screens/EyeScreen.tsx`, with types in `ui/src/api/types.ts` and `merge.ts`.
6. Add unit tests in `test/test_core/`.

`BlendPattern` is a worked example of a pattern built from two others.

## Design notes

- **Animation** runs in its own FreeRTOS task on a 5 ms tick (200 frames a second) and never blocks. The web server runs in other tasks; settings pass between them as a whole-struct copy under a mutex.
- **LED output** is hardware PWM (LEDC) at 5 kHz, 12-bit, gamma-corrected, one channel per LED.
- **Settings** are one structure with one JSON form, used by the REST API, the WebSocket, storage and `led-defaults.json`. Out-of-range values are clamped; an unknown pattern name is refused.
- **Storage** is written about two seconds after the last change, to spare the flash while a slider is dragged. Writing pauses the animation for about 40 ms.
- **Sign-in** uses a session cookie over plain HTTP. The password is stored as a salted PBKDF2-HMAC-SHA256 hash. Five failures lock sign-in for 30 seconds.
- **Updates** go to the spare one of two firmware slots (`min_spiffs.csv`, about 1.9 MB each; the firmware uses about 60% of one). The image is checked before the board switches to it.

### API

Every route except the first three needs a session.

| Method | Path | Purpose |
|---|---|---|
| GET | `/` | The interface |
| POST | `/api/login` | Sign in |
| GET | `/api/session` | Whether the caller is signed in |
| POST | `/api/logout` | Sign out |
| GET | `/api/state` | All LED settings, the pattern list, onboard LED availability |
| POST | `/api/config` | Change any subset of the LED settings |
| POST | `/api/reset` | LED settings back to the build's defaults |
| WS | `/ws` | Live LED state both ways |
| POST | `/api/credentials` | Change the sign-in details |
| GET, POST, DELETE | `/api/wifi` | Network status; join and save; forget |
| GET | `/api/wifi/scan` | Nearby networks |
| POST | `/api/wifi/ap` | Access point name and password |
| GET | `/api/system` | Version, uptime, memory, sizes |
| POST | `/api/update` | Upload a firmware image |
| POST | `/api/restart` | Restart |
| POST | `/api/factory-reset` | Restore factory settings |

## Known limitations

- The connection is not encrypted. Use the prop's own access point, with its password changed, for anything public.
- The board occasionally does not answer a brand-new connection. Browsers and the test client retry; a page that hangs once loads on reload.
- Only 2.4 GHz networks with a password or none. Networks that need a user name or a sign-in page cannot be joined.
- The interface has no automated browser tests. It is type-checked and linted in its build, and the API beneath it is tested.

## Recovering a board

If a board will not start after a bad upload, upload again over USB, holding BOOT as the upload starts if needed. To wipe every stored setting as well:

```powershell
pio run -t erase
pio run -t upload
```
