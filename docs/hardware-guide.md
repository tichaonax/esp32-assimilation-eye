# Assimilation Eye — Hardware Guide

How to build the eight-LED ring circuit on a breadboard, for either supported ESP32 board.

- For using and configuring the finished prop, see `docs/user-guide.md` (written in Phase 7).
- For building the firmware, see `README.md` (written in Phase 7).

> **Status:** Sections 1–8 are complete. Sections 9–11 are placeholders that are filled in once the firmware exists.

---

## 1. Supported boards

Both boards carry the same ESP-WROOM-32 module and run the same firmware. They differ only in how many pins they expose and what is printed next to them.

| | DOIT ESP32 DEVKIT V1 | ESP32-DevKitC (38-pin) |
|---|---|---|
| Status | **The board in hand** | Also supported |
| Header pins | 30 (15 per side) | 38 (19 per side) |
| Pin labels | `D23`, `D22`, `TX2`, `RX2` … | `IO23`, `IO22`, `IO17`, `IO16` … (some clones print `G23` or `23`) |
| Controllable onboard LED | Blue, on GPIO 2 | None on the genuine board; some clones have one on GPIO 2 |
| 5 V pin label | `VIN` | `5V` |
| BOOT button | Yes | Yes |

The board in hand was identified on 2026-10-09: 30 pins, with GPIO 17 and 16 labelled `TX2` and `RX2`. Over USB it reports chip ESP32-D0WD-V3 revision 3.1, 4 MB flash, MAC address `38:18:2b:b3:91:80`.

The wiring in section 7.1 was checked on this board on 2026-10-10 with the bring-up firmware: all eight LEDs light one at a time in the order LED 1 to LED 8, and the blue onboard LED on GPIO 2 blinks under firmware control.

**Wire by the printed label, not by counting positions.** The positions in this guide are a convenience for finding the label.

---

## 2. Bill of materials

| Qty | Part | Notes |
|---|---|---|
| 1 | ESP32 board | Either board from section 1 |
| 8 | Red 5 mm diffused LED | Forward voltage about 1.8–2.2 V, rated 20 mA. Diffused lenses give a wider viewing angle than clear ones |
| 8 | 220 Ω resistor, ¼ W | Colour bands red, red, brown, (gold). One per LED |
| 1 | Breadboard, full size (830 points) | These boards are wide; see section 7 |
| 9+ | Male-to-male jumper wires | Eight signal wires and one ground wire, plus spares |
| 1 | USB cable | USB-C or Micro-USB to match your board. It must be a data cable, not charge-only |
| 1 | USB power bank | For the wearable. Mains USB or a PC port is fine on the bench |

Optional:

| Qty | Part | Notes |
|---|---|---|
| 8 | 100 Ω resistor, ¼ W | Brown, black, brown. The brighter option in section 3 |
| 1 | Inline USB switch | An accessible way to power the prop off on the cap |
| 2–3 | Spare LEDs and resistors | LEDs are easy to damage while bending legs |

No transistors, driver chips or external power supply are needed. Section 4 explains why.

---

## 3. Resistor value

Each LED is driven straight from an ESP32 pin through its own resistor. The resistor sets the current.

**Assumptions**

- ESP32 pin output when on: 3.3 V
- Red LED forward voltage: 2.0 V typical (range 1.8–2.2 V)
- Target current: about 6 mA, which is clearly visible indoors and gentle on the board

**Calculation**

```
current = (pin voltage − LED forward voltage) / resistance
        = (3.3 V − 2.0 V) / 220 Ω
        ≈ 5.9 mA per LED
```

| | 220 Ω (standard) | 100 Ω (brighter) |
|---|---|---|
| Current per LED, typical | 5.9 mA | 13 mA |
| Current per LED, across the 1.8–2.2 V range | 5.0–6.8 mA | 11–15 mA |
| All eight LEDs fully on | about 47 mA | about 104 mA |
| Heat in each resistor | about 8 mW | about 17 mW |

Both columns are well within what a ¼ W (250 mW) resistor, a single ESP32 pin and the board's 3.3 V regulator can handle. The comet pattern lights only a few LEDs at once, most of them dimmed, so typical draw is far below the "all eight on" figure. Only the flash pattern at full brightness reaches it.

**Which to use:** start with 220 Ω. If the ring looks weak under stage lighting, swap all eight for 100 Ω. Do not go below 68 Ω.

Brightness is also adjustable from the web interface, but only downward from what the resistor allows.

---

## 4. Why no transistor drivers

A transistor driver is needed when a pin cannot supply the voltage or the current a load needs. Neither applies here.

- **Voltage:** a red LED needs about 2.0 V. The pin supplies 3.3 V, which leaves 1.3 V for the resistor to work with.
- **Current per pin:** about 6 mA (or 13 mA with 100 Ω), well under what an ESP32 pin can drive.
- **Total current:** 47–104 mA with everything on, a small part of what the board's regulator supplies.
- **One pin per LED:** no pin carries more than a single LED.

**When you would need drivers.** Blue, white and "true green" LEDs need 3.0–3.4 V, which is too close to 3.3 V: they light dimly and unevenly. The same applies if you want more than about 15 mA per LED, or several LEDs per pin. In those cases each LED gets one NPN transistor and is powered from the 5 V pin:

```
5 V pin (VIN / 5V) ──[ 150 Ω ]──►|── collector
                                LED      2N3904 (NPN)
ESP32 pin ──[ 1 kΩ ]──────────────── base
GND ──────────────────────────────── emitter
```

- LED resistor: (5 V − 3.1 V − 0.2 V) / 150 Ω ≈ 11 mA
- Base resistor: 1 kΩ
- The firmware is unchanged: pin high still means LED on.

This arrangement is for reference only. This project uses red LEDs with direct drive.

---

## 5. Pin assignment

The firmware uses the same eight GPIO numbers on both boards. All eight, and a ground pin, are on one edge of the board.

| Ring LED | GPIO | DEVKIT V1 label | DEVKIT V1 position | DevKitC 38-pin label | DevKitC position |
|---|---|---|---|---|---|
| LED 1 | 23 | `D23` | 1 | `IO23` | 2 |
| LED 2 | 22 | `D22` | 2 | `IO22` | 3 |
| LED 3 | 21 | `D21` | 5 | `IO21` | 6 |
| LED 4 | 19 | `D19` | 6 | `IO19` | 8 |
| LED 5 | 18 | `D18` | 7 | `IO18` | 9 |
| LED 6 | 17 | `TX2` | 9 | `IO17` | 11 |
| LED 7 | 16 | `RX2` | 10 | `IO16` | 12 |
| LED 8 | 4 | `D4` | 11 | `IO4` | 13 |
| Ground | — | `GND` | 14 | `GND` | 1 or 7 |

Positions are counted from the antenna end (the end opposite the USB connector) along the edge that carries pin 23.

**The full edge, for orientation**

DEVKIT V1, 15 pins, antenna end first. Pins used by this project are marked.

| Pos | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Label | `D23` | `D22` | `TX0` | `RX0` | `D21` | `D19` | `D18` | `D5` | `TX2` | `RX2` | `D4` | `D2` | `D15` | `GND` | `3V3` |
| Use | LED 1 | LED 2 | — | — | LED 3 | LED 4 | LED 5 | — | LED 6 | LED 7 | LED 8 | — | — | Ground | — |

DevKitC 38-pin, 19 pins, antenna end first.

| Pos | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Label | `GND` | `IO23` | `IO22` | `TXD0` | `RXD0` | `IO21` | `GND` | `IO19` | `IO18` | `IO5` | `IO17` | `IO16` | `IO4` | `IO0` | `IO2` | `IO15` | `SD1` | `SD0` | `CLK` |
| Use | Ground | LED 1 | LED 2 | — | — | LED 3 | (Ground) | LED 4 | LED 5 | — | LED 6 | LED 7 | LED 8 | — | — | — | — | — | — |

**Pins to leave alone**

| Pins | Reason |
|---|---|
| `TX0` / `RX0` (GPIO 1 and 3) | The USB serial link used for uploading and logs |
| `D5`, `D2`, `D15`, and GPIO 0 and 12 | Read by the chip at power-on to decide how to start. A load on them can stop the board booting or accepting uploads |
| `D2` (GPIO 2) | Also the onboard blue LED on the DEVKIT V1 |
| `3V3` | A power output, right next to `GND` on the DEVKIT V1. Nothing in this circuit connects to it. Shorting it to `GND` can damage the board |
| `SD0`–`SD3`, `CLK`, `CMD` (38-pin only) | Wired to the module's internal flash memory |
| `D34`, `D35`, `VP`, `VN` | Input only |

On the DEVKIT V1, `TX2` and `RX2` are simply GPIO 17 and 16. They are ordinary outputs here; the second serial port is not used.

---

## 6. LED polarity and ring layout

### 6.1 Polarity

An LED only lights when connected the right way round.

| | Anode (+) | Cathode (−) |
|---|---|---|
| Leg length (new LED) | Longer | Shorter |
| Rim of the lens | Round | Flat spot |
| Inside the lens | Smaller metal piece | Larger, cup-shaped piece |
| Connects to | Resistor, then the ESP32 pin | Ground |

A reversed LED does not light and, at these voltages, is not damaged. Turn it round.

### 6.2 The circuit for one LED

```
ESP32 pin ──[ 220 Ω ]──►|── GND
                      anode  cathode
                     (long)  (short)
```

Every LED has its own resistor. The resistor may go on either side of the LED; this guide puts it on the pin side. All eight cathodes join one common ground, which connects to one `GND` pin on the board.

### 6.3 Ring numbering

LED 1 is at the top of the lens. Numbering runs **clockwise as the audience sees it**, looking at the wearer's face. "Clockwise" in the web interface means this direction.

```
        Audience view                    Wearer's view (mirrored)

            LED 1                               LED 1
       LED 8     LED 2                     LED 2     LED 8

     LED 7   lens   LED 3                LED 3   lens   LED 7

       LED 6     LED 4                     LED 4     LED 6
            LED 5                               LED 5
```

| LED | Clock position, audience view |
|---|---|
| 1 | 12:00 |
| 2 | 1:30 |
| 3 | 3:00 |
| 4 | 4:30 |
| 5 | 6:00 |
| 6 | 7:30 |
| 7 | 9:00 |
| 8 | 10:30 |

On the breadboard, lay the LEDs out in a row in order 1 to 8. The order matters; the circle comes later, on the glasses.

---

## 7. Wiring

### 7.1 Connection table — DOIT ESP32 DEVKIT V1

| From (board label) | Through | To | Then |
|---|---|---|---|
| `D23` | 220 Ω | LED 1 anode (long leg) | LED 1 cathode → ground rail |
| `D22` | 220 Ω | LED 2 anode | LED 2 cathode → ground rail |
| `D21` | 220 Ω | LED 3 anode | LED 3 cathode → ground rail |
| `D19` | 220 Ω | LED 4 anode | LED 4 cathode → ground rail |
| `D18` | 220 Ω | LED 5 anode | LED 5 cathode → ground rail |
| `TX2` | 220 Ω | LED 6 anode | LED 6 cathode → ground rail |
| `RX2` | 220 Ω | LED 7 anode | LED 7 cathode → ground rail |
| `D4` | 220 Ω | LED 8 anode | LED 8 cathode → ground rail |
| `GND` | wire | ground rail | — |

### 7.2 Connection table — ESP32-DevKitC 38-pin

| From (board label) | Through | To | Then |
|---|---|---|---|
| `IO23` | 220 Ω | LED 1 anode (long leg) | LED 1 cathode → ground rail |
| `IO22` | 220 Ω | LED 2 anode | LED 2 cathode → ground rail |
| `IO21` | 220 Ω | LED 3 anode | LED 3 cathode → ground rail |
| `IO19` | 220 Ω | LED 4 anode | LED 4 cathode → ground rail |
| `IO18` | 220 Ω | LED 5 anode | LED 5 cathode → ground rail |
| `IO17` | 220 Ω | LED 6 anode | LED 6 cathode → ground rail |
| `IO16` | 220 Ω | LED 7 anode | LED 7 cathode → ground rail |
| `IO4` | 220 Ω | LED 8 anode | LED 8 cathode → ground rail |
| `GND` (position 1 or 7) | wire | ground rail | — |

### 7.3 Wiring diagram — DEVKIT V1

```
   antenna end
  ┌───────────┐
  │        D23├──[220Ω]──►|── LED 1 ──┐
  │        D22├──[220Ω]──►|── LED 2 ──┤
  │        TX0│  (leave free)         │
  │        RX0│  (leave free)         │
  │        D21├──[220Ω]──►|── LED 3 ──┤
  │        D19├──[220Ω]──►|── LED 4 ──┤
  │  ESP32 D18├──[220Ω]──►|── LED 5 ──┤   ground rail
  │         D5│  (leave free)         │   (breadboard "−" strip)
  │        TX2├──[220Ω]──►|── LED 6 ──┤
  │        RX2├──[220Ω]──►|── LED 7 ──┤
  │         D4├──[220Ω]──►|── LED 8 ──┤
  │         D2│  (leave free)         │
  │        D15│  (leave free)         │
  │        GND├───────────────────────┘
  │        3V3│  (leave free — do not short to GND)
  └───[USB]───┘
```

### 7.4 Wiring diagram — DevKitC 38-pin

```
   antenna end
  ┌────────────┐
  │         GND├───────────────────────┐
  │        IO23├──[220Ω]──►|── LED 1 ──┤
  │        IO22├──[220Ω]──►|── LED 2 ──┤
  │        TXD0│  (leave free)         │
  │        RXD0│  (leave free)         │
  │        IO21├──[220Ω]──►|── LED 3 ──┤
  │         GND│  (alternative ground) │
  │        IO19├──[220Ω]──►|── LED 4 ──┤   ground rail
  │  ESP32 IO18├──[220Ω]──►|── LED 5 ──┤   (breadboard "−" strip)
  │         IO5│  (leave free)         │
  │        IO17├──[220Ω]──►|── LED 6 ──┤
  │        IO16├──[220Ω]──►|── LED 7 ──┤
  │         IO4├──[220Ω]──►|── LED 8 ──┘
  │         IO0│  (leave free)
  │         IO2│  (leave free)
  │        IO15│  (leave free)
  │   SD1 … CLK│  (leave free)
  └───[USB]────┘
```

### 7.5 Power and ground

- **Power:** the board is powered through its USB connector, from a PC, a USB charger or a power bank. Nothing else needs power; the LEDs are fed by the pins.
- **Ground:** one wire from a board `GND` pin to the breadboard's ground rail. All eight LED cathodes go to that rail. This is the common ground.
- **Do not** connect the LEDs to `3V3`, `VIN` or `5V`.
- **Do not** power the board from USB and the `VIN`/`5V` pin at the same time.

---

## 8. Breadboard assembly

Work with the USB cable **unplugged** until step 9.

**How a breadboard is connected.** Each numbered row of five holes on one side of the centre channel is joined together. The long strips down the edges, marked `+` and `−`, are joined along their length. On many full-size boards these strips are split in the middle, so keep everything in one half or bridge the gap with a wire.

1. **Seat the board.** Push the ESP32 board into the breadboard across the centre channel, USB connector at one end and overhanging if you like. These boards are wide: you will have only one free hole beside each pin, and on some breadboards only along one edge. Position the board so the free holes are along the **edge that carries pin 23**. Every connection in this circuit is on that edge. Press evenly on both ends; do not rock it.
2. **Make the ground rail.** Run a jumper wire from the board's `GND` pin (on the pin 23 edge) to the `−` strip on that side of the breadboard.
3. **Check the LEDs.** Sort out eight LEDs and note the long leg on each. If legs have been trimmed, use the flat spot on the rim to find the cathode.
4. **Place the LEDs.** In a clear area of the breadboard, away from the ESP32 board, insert the eight LEDs in a line. Give each LED two rows of its own: long leg in one row, short leg in the next. Leave a row or two between LEDs. Keep them in order, LED 1 nearest the antenna end.
5. **Ground the cathodes.** For each LED, run a short jumper from the row holding its **short** leg to the `−` strip.
6. **Add the resistors.** For each LED, put one leg of a 220 Ω resistor in the row holding the LED's **long** leg, and the other leg in an empty row. Resistors have no polarity. Keep bare resistor legs from touching each other.
7. **Connect the signal wires.** For each LED, run a jumper from the free end of its resistor to the board pin in the table in section 7. Do them in order and say the label out loud as you go: `D23`, `D22`, `D21`, `D19`, `D18`, `TX2`, `RX2`, `D4`. Note the three gaps: skip `TX0` and `RX0` after `D22`, and skip `D5` after `D18`.
8. **Inspect before powering.**
   - Eight signal wires, eight resistors, eight LEDs, eight cathode wires, one ground wire.
   - Nothing connected to `3V3`, `VIN`/`5V`, `TX0`, `RX0`, `D5`, `D2` or `D15`.
   - No bare legs touching.
   - Every long leg on the resistor side, every short leg on the ground side.
9. **Power check, no firmware needed.** Plug in the USB cable. The board's red power LED should light. The ring LEDs will stay dark, or do whatever the firmware already on the board tells those pins to do.
10. **Polarity check, no firmware needed.** Unplug one signal wire from its board pin and touch that end to the `3V3` pin. That LED should light at normal brightness. Put the wire back on its pin and repeat for each LED. An LED that stays dark is reversed, or its cathode wire is missing. This is the one time anything touches `3V3`; take care not to brush the neighbouring `GND` pin.

**Where the two boards differ**

| Step | DEVKIT V1 (30-pin) | DevKitC 38-pin |
|---|---|---|
| 1 | 15 pins per side | 19 pins per side; needs more breadboard length |
| 2 | `GND` is second from the USB end, next to `3V3` | `GND` is the first pin at the antenna end; a second `GND` sits between `IO21` and `IO19` |
| 7 | Labels `D23` … `D4`, with `TX2` and `RX2` for LEDs 6 and 7 | Labels `IO23` … `IO4`, with `IO17` and `IO16` for LEDs 6 and 7 |
| 7 | Gaps: `TX0`, `RX0`, `D5` | Gaps: `TXD0`, `RXD0`, the middle `GND`, `IO5` |
| 9 | Blue onboard LED (GPIO 2) blinks when the firmware drives it; confirmed on the board in hand | No controllable onboard LED on the genuine board |

---

## 9. Initial testing with the firmware

*To be completed in Phase 7.* Will cover the power-on self-test, in which the firmware lights LEDs 1 to 8 in order so wiring order can be checked at a glance, and what a correct start-up looks like on the serial console.

Until then, the short version: on power-up the current firmware lights LED 1 to LED 8 in turn, fades the whole ring up and down once, then repeats a slow chase of one LED every 0.6 seconds while the blue onboard LED toggles. An LED that stays dark is reversed or missing its ground wire. LEDs lighting out of order means two signal wires are swapped.

Steps 9 and 10 of section 8 confirm power and polarity without any firmware.

---

## 10. Troubleshooting

*To be completed in Phase 7.* Hardware troubleshooting will live in `docs/user-guide.md` alongside the software troubleshooting, laid out as symptom, likely cause and fix. This section will link to it.

Quick checks available now:

| Symptom | Check |
|---|---|
| One LED never lights in the polarity check | LED reversed; cathode wire missing; resistor leg not seated |
| No LED lights in the polarity check | Ground wire from `GND` to the `−` strip missing; `−` strip split in the middle of the breadboard |
| Board's power LED is off | Cable is charge-only or faulty; try another cable or port |
| Board gets warm or the PC reports a USB fault | Unplug at once. Look for a short between `3V3` and `GND`, or bare legs touching |

---

## 11. Transferring to the wearable

*To be completed in Phase 7.* Will cover arranging the LEDs around the lens in the order in section 6.3, soldering and insulating joints, strain relief, mounting the board and power bank on the cap, keeping the BOOT button clear, and an accessible way to switch the prop off.
