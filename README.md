# Meow MHz (MM) 🐱📡

<img width="2816" height="1536" alt="Gemini_Generated_Image_gqk5jngqk5jngqk5" src="https://github.com/user-attachments/assets/f8142811-7005-410a-93ad-8e3aa2638902" />


**A tiny, fully open, sub-GHz capture-and-replay tool with a cat.**
Built on a cheap ESP32-S3 — roughly **$12 in parts** — by CyberMeow(me).

Meow MHz captures OOK/ASK sub-GHz signals (the kind used by cheap remotes),
shows you the actual waveform on a color screen, finds what frequency a signal
is on, and replays saved signals from flash. It is not a Flipper Zero — it does
*one* thing (sub-GHz) and does it openly and cheaply.

---

## What it does

- **Read** — capture an OOK signal on 14 common frequencies, auto-triggered, with a live waveform preview and edge count.
- **Analyze** — a signal finder: hop through the bands, hold your remote, and it tells you which frequency it's transmitting on.
- **Transmit** — replay a captured or loaded signal.
- **Saved** — a flash-backed library of signals that survive power-off; browse, replay, and delete them on-device.
- A black-and-green phosphor UI, a built-in CC1101 self-test, and a cat companion that reacts to what you're doing.

## What it does *not* do (be honest with yourself)

| | Meow MHz |
|---|---|
| Fixed-code remotes (old garage doors, fans, doorbells, cheap remotes) | ✅ capture + replay |
| Rolling codes (KeeLoq, modern cars, modern garage openers) | ❌ **impossible by design** — the code changes every press, so a replay is dead on arrival |
| NFC / RFID / Infrared / iButton | ❌ not this tool |
| Protocol decoding (naming a signal's protocol) | ❌ raw waveform only, for now |

If you need a do-everything multitool, buy a Flipper Zero. Meow MHz is the cheap,
open, sub-GHz corner of that — with a cat.

---

## Hardware / Bill of Materials

Approximate — prices move, so treat these as a guide. AliExpress is where this
project is meant to live; Amazon is roughly double.

| Part | Approx. (AliExpress) |
|---|---|
| ESP32-S3 Super Mini | ~$4–6 |
| ST7789 1.14" 135×240 display | ~$3–5 |
| CC1101 sub-GHz module | ~$3–5 |
| 5× momentary tactile buttons | cents |
| Antenna wire (see below) | scrap wire |
| **Total** | **~$12** |

> ⚠️ **CC1101 quality on cheap modules is a coin flip** (wrong chip, cold joints,
> DOA). The firmware runs a self-test on boot and shows **`CC1101 OK`** or
> **`CC1101 FAIL`** on the splash — check it first before assuming anything's broken.

### Antenna — do not skip

The CC1101 needs an antenna or it's nearly deaf. Solder a wire to the `ANT` pad:
- **~17.3 cm** for 433 MHz
- **~23 cm** for 315 MHz

Without it you get a few inches of range and weak transmit, and you'll think the tool is broken when it isn't.

---

## Wiring

Two separate SPI buses — the display and the radio do **not** share a bus.
Full details and notes in **[docs/WIRING.md](docs/WIRING.md)**.

**Display** (HSPI):

| Display | ESP32-S3 |
|---|---|
| SCK | GPIO 12 |
| MOSI/SDA | GPIO 11 |
| CS | GPIO 10 |
| DC | GPIO 9 |
| RST | GPIO 8 |
| BLK | GPIO 7 (or 3V3) |
| VCC / GND | 3V3 / GND |

**CC1101** (FSPI):

| CC1101 | ESP32-S3 |
|---|---|
| SCK | GPIO 14 |
| SI (MOSI) | GPIO 15 |
| SO (MISO) | GPIO 16 |
| CSN (CS) | GPIO 17 |
| GDO0 | GPIO 18 |
| GDO2 | not connected |
| VCC / GND | 3V3 (**not 5V**) / GND |

**Buttons** — each is just one leg to the GPIO, one leg to GND (internal pull-ups, no resistors):

| Button | GPIO |
|---|---|
| UP | 1 |
| DOWN | 2 |
| OK | 4 |
| BACK | 5 |
| FN | 6 |

---

## Build & flash

1. Install the **ESP32 board package** (`esp32` by Espressif) via Boards Manager.
2. Install libraries via Library Manager:
   - **Adafruit ST7735 and ST7789 Library** (pulls in Adafruit GFX + BusIO)
   - **SmartRC-CC1101-Driver-Lib** (by LSatan — provides `ELECHOUSE_CC1101_SRC_DRV.h`)
3. Open `firmware/MeowMHz/MeowMHz.ino`.
4. Tools settings:
   - Board: **ESP32S3 Dev Module**
   - USB CDC On Boot: **Enabled**
   - PSRAM: **Disabled**
5. Plug in over USB-C, pick the **Port**, hit **Upload**. If it doesn't catch on its own, replug the board right as it says "Connecting…".
6. Open **Serial Monitor at 115200** — you should see `MM ready`. You can drive the whole UI over serial too: `w`/`s` move, `e` select, `b` back, `f` fn.

> The CC1101 can stay connected while flashing — none of its pins are boot-strapping or USB pins.

---

## Using it

**Controls:** 5 buttons, mirrored on serial → `w`=up, `s`=down, `e`=ok/select, `b`=back, `f`=fn.

- **Find a signal's frequency:** Menu → **Analyze** → hold your remote button down → watch which bar spikes → **OK** locks that frequency for Read/Transmit.
- **Capture:** Menu → **Read** → **OK** to listen → fire your remote → it auto-captures and draws the waveform.
- **Save:** on the captured screen, press **FN** — it's written to flash and survives power-off.
- **Replay:** Menu → **Transmit** → **OK** (transmits 5×). Or Menu → **Saved** → pick a signal → **OK** replays it directly.
- **Manage:** in **Saved**, scroll with up/down, **FN** deletes.

> **First boot after flashing formats the flash** (one-time) — `Saved` will start empty.
> ⚠️ **Reflashing the firmware can wipe saved signals**, since they live in the ESP's
> internal flash alongside the firmware. An SD card would be the clean fix for
> saves that outlive firmware updates (not implemented yet).

---

## Roadmap

**Toward 1.0**
- [x] Sub-GHz capture / analyze / replay
- [x] Flash-backed signal library
- [ ] Battery + charging (single LiPo + TP4056)
- [ ] Enclosure
- [ ] Tuned antenna guidance
- [ ] Full build documentation

**Beyond 1.0 (help wanted)**
- Signal naming / labeling
- Protocol decoding (identify Princeton / CAME / etc.)
- SD card storage option

Forks that add capabilities beyond sub-GHz (WiFi, BLE, IR) are welcome — that's
what open hardware is for. This repo stays focused on being the clean sub-GHz base.

---

## Contributing

PRs, issues, and hardware builds welcome — see **[CONTRIBUTING.md](CONTRIBUTING.md)**.
If you build a derivative firmware, please credit CyberMeow (see License).

## License

- **Code** — [MIT](LICENSE). Do what you like, including sell it; just keep the copyright notice, which is how CyberMeow gets credited in derivatives.
- **Docs, wiring, and artwork** — [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) (attribution).

## Acknowledgments

Designed and built by **Amrio / CyberMeow**. Development done with help from
Claude (Anthropic) as a coding and design assistant.

---

## ⚠️ Legal & responsible use

Meow MHz can transmit on sub-GHz frequencies. **You are responsible for using it
legally.** Radio transmission is regulated (FCC in the US, and equivalents
elsewhere) — some frequencies and power levels require a license, and some are
off-limits. Only transmit on frequencies you are permitted to use, and only
capture or replay signals from **devices you own or have explicit permission to
test**. This project is for security research, education, and testing your own
equipment. Don't use it to interfere with, access, or disrupt anything that
isn't yours. The authors take no responsibility for misuse.
