# Assimilation Eye — User Guide

How to power, connect to, configure and troubleshoot the prop. A phone is enough for everything except changing the built-in LED defaults (section 4.7), which needs the PC with the project on it.

- To build the circuit, see [hardware-guide.md](hardware-guide.md).
- To build or change the firmware, see [README.md](../README.md).

## Contents

1. [Quick reference](#1-quick-reference)
2. [First power-on](#2-first-power-on)
3. [Connecting and signing in](#3-connecting-and-signing-in)
4. [The Eye screen](#4-the-eye-screen)
5. [The Wi-Fi screen](#5-the-wi-fi-screen)
6. [The System screen](#6-the-system-screen)
7. [Factory reset with the BOOT button](#7-factory-reset-with-the-boot-button)
8. [At the demo venue](#8-at-the-demo-venue)
9. [Pre-demo checklist](#9-pre-demo-checklist)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. Quick reference

Factory settings. All of them can be changed, and a factory reset restores them.

| | |
|---|---|
| Prop's own Wi-Fi network | `AssimilationEye` |
| Password for that network | `resistanceisfutile` |
| Address on that network | `http://192.168.4.1` |
| Sign-in user name | `admin` |
| Sign-in password | `assimilate` |
| Name on a joined network | `http://assimilation-eye.local` |
| Factory reset | Hold the BOOT button for 10 seconds |

The address always starts with `http://`, not `https://`.

---

## 2. First power-on

1. Plug the board into USB power: a power bank, a phone charger or a PC.
2. The board's red power light comes on.
3. **Self-test.** LEDs 1 to 8 light one after another, clockwise as the audience sees the ring, then the whole ring fades up and down once. This takes about three seconds.
4. The ring starts its pattern. A new or factory-reset prop starts on **Comet and flash**.
5. About five seconds after power-on the prop's own Wi-Fi network, `AssimilationEye`, is available.

If the self-test skips an LED or runs out of order, see [Troubleshooting](#102-hardware).

The prop needs no phone to run. It starts the pattern and settings it was last left with.

---

## 3. Connecting and signing in

There are two ways to reach the prop. Both show the same screens and take the same sign-in.

### 3.1 Through the prop's own network (always works)

1. On the phone, open the Wi-Fi settings and join `AssimilationEye` with the password `resistanceisfutile`.
2. The phone will say the network has no internet. That is correct. If it asks whether to stay connected, choose to stay.
3. Open a browser and go to `http://192.168.4.1`.

If the page does not load, turn off mobile data for a moment. Some phones send everything over mobile data when Wi-Fi has no internet.

### 3.2 Through a network the prop has joined

Once the prop has joined a Wi-Fi network (section 5), any phone or PC on that network can open it at:

- the address shown on the Wi-Fi screen under **Joined network**, for example `http://192.168.1.50`, or
- `http://assimilation-eye.local`. This name works on iPhones, Macs and recent Windows. Many Android phones do not support it; use the address.

The prop's own network stays on as well, so section 3.1 always remains available.

### 3.3 Signing in

Enter the user name and password. On a new prop these are `admin` and `assimilate`.

- A banner reminds you while the password is still the factory default. Change it on the System screen (section 6.3).
- After **five wrong attempts** sign-in is locked for **30 seconds**. The screen counts down.
- A sign-in lasts until the prop restarts, until you sign out, or 12 hours without use.

### 3.4 Finding your way around

Three tabs across the top: **Eye**, **Wi-Fi** and **System**.

The pill at the top right shows the link to the prop:

| Pill | Meaning |
|---|---|
| Live | Changes take effect at once |
| Connecting… | The page has just opened |
| Reconnecting… | The page has lost the prop and is trying again. The LEDs carry on regardless. Controls are greyed out until it is back |

Changes apply immediately; there is no Save button on the Eye screen. The prop stores a change about two seconds after you stop adjusting.

---

## 4. The Eye screen

### 4.1 Ring

| Control | What it does |
|---|---|
| **Stop the ring / Start the ring** | Turns the ring off and on. The setting is remembered |
| **Pattern** | Rotating comet, Synchronized flash, or Comet and flash |
| **Overall brightness** | Scales everything, 0–100% |

The cards below change with the pattern chosen.

### 4.2 Rotating comet

One bright LED travels around the ring, followed by a fading tail.

| Control | Range | What it does |
|---|---|---|
| **Direction** | Clockwise / Counter-clockwise | As the audience sees the ring |
| **Speed** | 20 ms to 2 s per LED | Time the head spends on each LED. The hint shows the time for one lap |
| **Head brightness** | 0–100% | The leading LED |
| **Tail length** | 0–7 LEDs | LEDs behind the head. 4 gives five lit LEDs in all |
| **Tail fade** | 10–90% dimmer per LED | How quickly the tail dies away. Higher is a shorter-looking tail |

The head glides from one LED to the next, so even slow speeds look smooth.

### 4.3 Synchronized flash

All eight LEDs pulse together.

| Control | Range | What it does |
|---|---|---|
| **Interval** | 0.1–10 s | From the start of one flash to the start of the next |
| **Ramp up** | Abrupt to 5 s | Time to rise from minimum to maximum |
| **Hold at maximum** | 0–10 s | Time spent at full |
| **Ramp down** | Abrupt to 5 s | Time to fall back |
| **Maximum brightness** | 0–100% | The top of the pulse |
| **Minimum brightness** | 0–100% | The level between pulses. 0 is dark |

Ramp up, hold and ramp down must fit inside the interval. If they do not, the prop shortens the hold first, then the ramps, and the sliders move to show what it settled on. The minimum can never be above the maximum.

For a hard strobe, set both ramps to Abrupt. For a slow breathing glow, use long ramps and a short hold.

### 4.4 Comet and flash

Plays the comet, crossfades into the flash, and crossfades back, for as long as it runs. It uses the comet and the flash exactly as you have set them, and both cards stay on screen so they can be adjusted while the combined pattern plays.

| Control | Range | What it does |
|---|---|---|
| **Time on comet** | 1–60 s | How long the comet plays each round |
| **Time on flash** | 1–60 s | How long the flash plays each round |
| **Crossfade** | Cut to 5 s | How long each blend between the two takes |
| **Comet direction** | Random / As set for the comet | See below |

With **Random**, the comet's direction is picked afresh each time it returns. The choice is made while the flash is showing, so the comet never visibly reverses. The Direction control on the Comet card is hidden while Random is on. With **As set for the comet**, the comet always turns the way chosen on the Comet card.

### 4.5 Onboard LED

The small blue LED on the board itself. It is separate from the ring. A board without one shows a note instead of these controls.

| Behaviour | What it does |
|---|---|
| **Off** | Stays dark |
| **On** | Stays lit |
| **Heartbeat** | A double beat, repeating, to show the prop is alive |
| **Blink on change** | Blinks when the ring pattern is switched. Blinks and Blink length set how many and how long |

### 4.6 Reset LED settings

At the bottom of the screen. Tap once, then tap again to confirm. It restores every setting on this screen to the defaults this firmware was built with. Sign-in and Wi-Fi are not touched.

### 4.7 Making your settings the defaults (`led-defaults.json`)

The defaults are the settings a new or factory-reset prop starts with, and the ones **Reset LED settings** returns to. They are fixed when the firmware is built, from a file called `led-defaults.json` in the project folder. Changing them is the one job in this guide that needs the PC with the project on it.

The file covers everything on the Eye screen: whether the ring is running, the pattern, overall brightness, and every comet, flash, combined-pattern and onboard LED setting. Sign-in and Wi-Fi details are not part of it.

**Step 1. Tune the prop.** On the Eye screen, set everything the way you want a fresh prop to behave, including which pattern is selected.

**Step 2. Capture the settings.** On the PC, open PowerShell in the project folder and run the command below with the prop's address: `192.168.4.1` if the PC is on the prop's own network, or the address from the Wi-Fi screen.

```powershell
python scripts\save_led_defaults.py --device 192.168.4.1
```

It asks for the sign-in password, writes `led-defaults.json` and lists each setting that changed. To look without writing anything, add `--show`. If the user name is not `admin`, add `--user NAME`.

**Step 3. Build and upload the firmware.**

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload
```

Near the top of the output, this line confirms the file was used:

```
led_defaults: CUSTOM LED DEFAULTS - 22 settings from led-defaults.json
```

**Step 4. Check.** On the Eye screen tap **Reset LED settings**. The prop goes to the settings you captured. The System screen shows **LED defaults: Set for this build**.

The upload by itself does not change what the ring is doing, because the prop keeps the settings it already had stored. The reset in step 4 is what shows the new defaults.

**Editing the file by hand.** It is plain text and can be opened in any editor. For example, to change only the starting pattern, edit the `pattern` line and repeat steps 3 and 4:

| `"pattern"` value | Starts on |
|---|---|
| `"comet"` | Rotating comet |
| `"flash"` | Synchronized flash |
| `"blend"` | Comet and flash |

Any setting may be left out of the file; it then keeps its built-in value. A misspelt setting or a value out of range stops the build with a message naming it, for example `comet.tailLength is 12; it must be from 0 to 7`. Nothing is uploaded until the file is corrected.

**Going back to the built-in defaults.** Delete `led-defaults.json`, then build and upload again.

**Keeping the file.** Commit `led-defaults.json` with the rest of the project so every later build uses it.

---

## 5. The Wi-Fi screen

### 5.1 Joining a network

1. Tap **Search for networks**. After a few seconds a list appears with signal strength.
2. Tap the network. Its name is filled in. A network that is hidden can be typed in instead, exactly as it is spelt.
3. Enter the password. Leave it empty for an open network.
4. Leave **Address** on **Automatic (DHCP)** unless you need a fixed address.
5. Tap **Save and join**.

Within about 20 seconds the **Joined network** card at the top shows the result. When joined, it shows the address to use from that network.

If you are connected through the prop's own network while doing this, the page may lose contact for a moment while the prop changes channel. It reconnects by itself.

The prop only sees **2.4 GHz** networks. A network that asks for a user name as well as a password, or shows a sign-in web page (common in hotels and offices), cannot be joined.

### 5.2 A fixed (static) address

Choose **Static** under Address and fill in:

| Field | Example | Notes |
|---|---|---|
| IP address | `192.168.1.50` | Pick one outside the range the router hands out by itself |
| Subnet mask | `255.255.255.0` | |
| Gateway | `192.168.1.1` | Usually the router's address. Must be in the same subnet as the IP address |
| DNS server | | Optional. Empty means use the gateway |
| Second DNS server | | Optional |

Settings that cannot work are refused with a message saying why. Addresses in `192.168.4.x` are not allowed, because the prop's own network uses them.

A static address that is valid but wrong for the network, for example one already used by another device, cannot be detected. If the prop becomes unreachable there, connect through its own network (section 3.1) and correct it.

To go back, choose **Automatic (DHCP)** and tap **Save and join**. For the network already saved, the password can be left empty to keep it.

### 5.3 If the network is unavailable

A wrong password, or a saved network that is out of range, does no harm. The card says it could not join and why, the prop keeps trying every half minute, and it stays reachable on its own network throughout.

**Forget this network** removes the saved network.

### 5.4 The prop's own access point

The last card changes the name and password of the prop's own network. The password must be 8 to 63 characters. Saving disconnects every device on that network; join again with the new details.

A warning shows while the factory password is still in use.

---

## 6. The System screen

### 6.1 This eye

Firmware version and build time, whether the LED defaults were set for this build, running time, the reason for the last start, memory and firmware size.

A warning appears on a development build. Such a build has a Wi-Fi name and password compiled in and must not be passed on.

### 6.2 Update firmware

1. Get the `firmware.bin` file from whoever built the firmware.
2. Tap the file chooser and select it.
3. Tap **Install this firmware**, then tap again to confirm.
4. Keep the page open. The ring pulses slowly while the firmware is installed.
5. The prop restarts and the sign-in screen returns. The self-test runs again.

LED settings, sign-in details and the saved network are all kept.

A file that is not firmware for this board, or is too large, is refused with a message, and the prop carries on with the firmware it has. An upload that is interrupted also leaves the running firmware in place; start again.

### 6.3 Sign-in details

Enter the current password, then the user name (3 to 32 characters) and the new password twice (8 to 64 characters). Saving signs out every other browser.

If the new details are lost, a factory reset (section 7) restores `admin` and `assimilate`.

### 6.4 Signed in

**Sign out** ends the session in this browser.

### 6.5 Restart and reset

- **Restart the eye** restarts it. Nothing is lost.
- **Factory reset** needs the current password. It does the same as holding the BOOT button, described next.

---

## 7. Factory reset with the BOOT button

Use this when the sign-in details are lost or the prop is unreachable.

1. With the prop powered and running, press and hold the **BOOT** button on the board.
2. After 3 seconds the whole ring starts to blink as a warning. Letting go now cancels.
3. Keep holding. At 10 seconds the ring lights steadily for two seconds. Let go.
4. The prop restarts.

What it restores:

| | Restored to |
|---|---|
| Sign-in | `admin` / `assimilate` |
| Saved Wi-Fi network | Forgotten |
| Prop's own network | `AssimilationEye` / `resistanceisfutile` |
| LED settings | The defaults this firmware was built with |

The firmware itself is not changed.

**Do not hold BOOT while plugging in the power.** That puts the board into its programming mode: the ring stays dark and nothing runs. Unplug it and plug it in again without touching the button.

---

## 8. At the demo venue

**Use the prop's own network.** It needs nothing from the venue, it cannot be blocked by the venue's Wi-Fi rules, and no one else is on it.

1. Power the prop.
2. Join `AssimilationEye` from the phone that will control it, and open `http://192.168.4.1`.
3. Sign in, set the pattern, and leave the page open.

Practical points:

- **Range** is a room, not a building. Keep the controlling phone within about ten metres.
- **Stop the phone leaving the network.** Tell it to stay connected when it warns there is no internet, and turn off any "switch to mobile data" option for the duration.
- **The prop does not need the phone.** If the phone drops off, the ring carries on with the last settings. Reconnect when convenient.
- **Switching pattern mid-demo** is one tap on the Eye screen. For a dramatic start, tap **Stop the ring** beforehand, put the prop on, then tap **Start the ring**.
- **Before unplugging** after changing a setting, wait two seconds so the change is stored.
- **Do not use the venue's Wi-Fi for control** unless you have tested it there. Many venue networks stop devices from talking to each other.

**A word on security.** The connection is not encrypted. On a shared network, someone able to capture traffic could read the password as you sign in. On the prop's own network, with its password changed from the factory one, only people you have given that password to can connect. Change both passwords before a public demo.

---

## 9. Pre-demo checklist

The day before:

- [ ] Power bank fully charged, and tested with the prop for at least 15 minutes without cutting out
- [ ] Sign-in password changed from the factory default
- [ ] Access point password changed from the factory default, and the new one saved on the controlling phone
- [ ] Pattern and settings chosen, and the prop switched off and on once to confirm it comes back to them
- [ ] All eight LEDs light in the self-test, in order
- [ ] Wiring and joints secure; nothing pulls when the cap or glasses move

On the day:

- [ ] Phone joined to the prop's network and the page open at `http://192.168.4.1`
- [ ] Pill at the top right reads **Live**
- [ ] Phone set not to sleep or switch network during the demo
- [ ] Ring stopped or running, as the script needs
- [ ] The off switch is within reach

---

## 10. Troubleshooting

### 10.1 Connecting, signing in and updating

| Symptom | Likely cause | Fix |
|---|---|---|
| `AssimilationEye` is not in the phone's Wi-Fi list | The prop is not powered or has not finished starting | Check the board's red power light. Wait ten seconds after power-on and refresh the list |
| | The network name was changed | Look for the new name, or factory reset (section 7) |
| | The board is in programming mode because BOOT was held at power-on | Unplug and plug in again without touching BOOT |
| The phone will not join `AssimilationEye` | Wrong password | The factory password is `resistanceisfutile`. If it was changed and lost, factory reset |
| | The phone remembers an old password | Forget the network on the phone and join again |
| Joined, but `192.168.4.1` does not load | The phone is using mobile data because the network has no internet | Turn off mobile data, or tell the phone to stay on this network |
| | The browser changed the address to `https://` | Type `http://192.168.4.1` in full |
| | A VPN or private relay is on | Turn it off for this network |
| The page loads slowly or hangs once, then is fine | The board occasionally misses a new connection | Reload the page |
| `assimilation-eye.local` does not open | The phone or PC does not support `.local` names (many Android phones) | Use the numeric address from the Wi-Fi screen |
| | The phone is not on the same network as the prop | Join the same network, or use the prop's own |
| The address that worked yesterday does not work | The router gave the prop a different address | Read the new address from the Wi-Fi screen through the prop's own network. For an address that never changes, set a static one (section 5.2) |
| "Too many wrong attempts" | Five wrong sign-ins | Wait 30 seconds |
| The user name or password is forgotten | — | Factory reset (section 7) |
| Signed out unexpectedly | The prop restarted, or the sign-in details were changed from another browser | Sign in again |
| Pill shows **Reconnecting…** | The phone left the network or moved out of range | Rejoin the network. The ring is still running |
| The prop will not join a network | Wrong password | Enter it again. The card says "the network refused the password" when this is the cause |
| | The network is 5 GHz only | Use a 2.4 GHz network, or a phone hotspot set to 2.4 GHz |
| | The network needs a user name or a sign-in web page | It cannot be joined. Use the prop's own network |
| Joined, but the prop cannot be reached at its address | The network stops devices talking to each other | Use the prop's own network |
| | A static address clashes or is in the wrong range | Correct it through the prop's own network, or switch back to DHCP |
| Firmware upload is refused | The file is not a firmware for this board, or it is too large | Use the `firmware.bin` from the build. The prop is unharmed |
| Firmware upload stops part-way | The connection dropped | The previous firmware is still running. Move closer and upload again |
| A setting did not survive switching off | Power was removed within two seconds of the change | Make the change again and wait before unplugging |
| The ring hesitates for an instant about two seconds after a change | The setting is being stored | Normal. It does not happen while nothing is being changed |

### 10.2 Hardware

| Symptom | Likely cause | Fix |
|---|---|---|
| Nothing at all: no red power light | No power reaching the board | Try another cable, another port or another power bank. Check the inline switch |
| | The cable is damaged | Replace it |
| Red power light is on, the ring is dark, no Wi-Fi network | The board is in programming mode because BOOT was pressed at power-on | Unplug and plug in again without touching BOOT. On the wearable, check nothing is pressing on the button |
| Red light on, Wi-Fi network present, the ring is dark | The ring is stopped, or brightness is at 0 | On the Eye screen tap **Start the ring** and raise **Overall brightness** |
| | The ground wire from the board to the LEDs has come off | Reconnect `GND` to the common cathode wire |
| One LED never lights, including in the self-test | The LED is the wrong way round | Turn it: long leg to the resistor, short leg to ground |
| | A loose wire, resistor leg or solder joint | Reseat or re-solder. Move the wiring gently while watching the LED |
| | The LED is dead | Swap it with a neighbour. If the fault moves with the LED, replace it |
| The self-test lights the LEDs out of order | Two signal wires are swapped | Compare against the table in section 7 of the hardware guide and swap them back |
| The comet runs the wrong way round for the audience | The ring was assembled in mirror image | Choose the other **Direction** on the Eye screen. Nothing needs rewiring |
| All LEDs are dim | Brightness settings are low | Raise **Overall brightness**, and **Head brightness** or **Maximum brightness** |
| | 220 Ω is not bright enough under stage lighting | Change all eight resistors to 100 Ω (hardware guide, section 3) |
| One LED is dimmer than the rest | A resistor of the wrong value | Check the colour bands: red, red, brown for 220 Ω |
| | A poor joint adding resistance | Re-solder it |
| An LED flickers or cuts out when the prop moves | A loose connection or a wire breaking at a joint | Re-solder, and add strain relief so the wire does not flex at the joint |
| Two LEDs always light together | Their signal wires or legs are touching | Separate and insulate them |
| The ring looks striped or flickers on camera | The camera's shutter is beating with the LED dimming | Raise the brightness, which reduces the effect, or change the camera's shutter speed |
| The prop switches off after half a minute to a few minutes on a power bank | The power bank shuts down because the prop draws too little current for it to notice | Use a power bank with a low-current or "always on" mode, or a different power bank. Test for 15 minutes before the demo |
| The prop restarts by itself | A loose USB plug, or a weak power bank | Reseat the plug; try another cable or bank. The System screen shows the reason for the last start |
| The board is hot, or the PC reports a USB fault | A short circuit | Unplug at once. Look for bare wires touching, especially between `3V3` and `GND` |
| The PC does not detect the board | A charge-only USB cable | Use a data cable |
| | The driver for the board's USB chip (CP210x) is missing | Install the CP210x driver from Silicon Labs |
| Uploading from the PC fails with "Failed to connect" | The board did not enter programming mode by itself | Hold BOOT while the upload starts, and let go when it begins writing |
| The blue onboard LED never lights | Its behaviour is set to Off | Choose another behaviour on the Eye screen |
| | The board has none (genuine 38-pin DevKitC) | Nothing to fix. The Eye screen says so |

### 10.3 When nothing else works

1. Unplug the prop, wait five seconds, plug it in again without touching any button.
2. Watch the self-test. If it runs, the board, firmware and LEDs are fine and the problem is in connecting.
3. Connect through the prop's own network (section 3.1).
4. If you cannot sign in, factory reset (section 7).
5. If there is no self-test and no Wi-Fi network, the firmware needs to be put back from a PC over USB. See [README.md](../README.md).
