# Meow MHz (MM) 🐱📡
<img width="2816" height="1536" alt="Gemini_Generated_Image_gqk5jngqk5jngqk5" src="https://github.com/user-attachments/assets/fe39ab7a-925a-44f8-87ae-769506785f30" />


**The sub-GHz tool you learn on.**
A tiny, fully open, capture-and-replay device with a cat — built on a cheap
ESP32-S3 for roughly **$12 in parts**, by [CyberMeow](https://github.com/).

**Release 1.0** · [Changelog](CHANGELOG.md) · New to this? → **[QUICKSTART](QUICKSTART.md)**
---

Most sub-GHz tools are built for people who already know RF. Meow MHz is built
for your **first** time: catch a signal from a remote, see it on screen, and play
it back. That's the whole idea. It does one thing — sub-GHz — and tries to make
that one thing friendly, cheap, and open.

It is **not** a Flipper Zero, and doesn't try to be. It's the thing you use
*before* one.

## What it does

- **Read** — capture an OOK signal (the kind cheap remotes use) on 14 common frequencies, auto-triggered, with a live waveform you can actually see.
- **Analyze** — hold a remote and it tells you which frequency it's on.
- **Transmit** — play a captured signal back.
- **Saved** — a library of signals kept in flash that survive power-off; browse, replay, delete on the device.
- **Web UI** — manage it from your phone or PC browser (no app to install): send signals, download them to your device, import them back.
- A friendly black-and-green screen, a self-test that tells you if the radio is wired right, and a cat that reacts to what you're doing.

## Kept simple on purpose

MM deliberately leaves out the advanced stuff so it stays easy to learn:

| | Meow MHz |
|---|---|
| Fixed-code remotes (old garage doors, fans, doorbells, cheap remotes) | ✅ capture + replay |
| Rolling codes (modern cars, modern garage openers) | ❌ **impossible for any tool** — the code changes every press |
| NFC / RFID / Infrared / iButton / BLE | ❌ not this tool — that's what a Flipper is for |
| Protocol decoding, brute-force, multi-modulation | ❌ left out to keep it beginner-friendly |

When you outgrow MM, you'll know — and then you graduate to a Flipper Zero or
flash [Bruce](https://github.com/pr3y/Bruce) onto more capable hardware. MM is
the on-ramp, not the destination. That's the point.

---

## Hardware / Bill of Materials

Approximate prices — they move, so treat as a guide. Buy from AliExpress (Amazon
is roughly double for the same parts).

| Part | Approx. (AliExpress) |
|---|---|
| ESP32-S3 Super Mini | ~$4–6 |
| ST7789 1.14" 135×240 display | ~$3–5 |
| CC1101 sub-GHz module | ~$3–5 |
| 5× momentary tactile buttons | cents |
| LiPo battery — **500 mAh or larger** (see note) | ~$3–5 |
| Antenna wire | scrap wire |
| **Total** | **~$12–15** |

> ⚠️ **CC1101 quality on cheap modules is a coin flip.** The firmware self-tests on
> boot and shows **`CC1101 OK`** or **`CC1101 FAIL`** on the splash — check that
> first before assuming anything's broken.

> 🔋 **Use a 500 mAh+ battery.** Tiny cells (under ~300 mAh) can't supply the
> current spike when WiFi starts and the board will brown out and reset. A 500–1000
> mAh LiPo or an 18650 fixes it. Mount it behind the board to keep things tidy.

### Antenna — don't skip it

Solder a wire to the CC1101's `ANT` pad: **~17.3 cm** for 433 MHz, **~23 cm** for
315 MHz. Without one, range is a few inches and you'll think it's broken when it
isn't.

---

## Wiring

Two separate SPI buses — display and radio don't share. Full pinout with notes:
**[docs/WIRING.md](docs/WIRING.md)**.

**Buttons** are the simplest part — one leg to the GPIO, one leg to GND, no
resistors: UP→1, DOWN→2, OK→4, BACK→5, FN→6.

**Display (HSPI):** SCK 12, MOSI 11, CS 10, DC 9, RST 8, BLK 7/3V3.
**CC1101 (FSPI):** SCK 14, SI 15, SO 16, CSN 17, GDO0 18, VCC 3V3 (**not 5V**).

---

## Build & flash

1. Install the **ESP32 board package** (`esp32` by Espressif) in Boards Manager.
2. Install libraries (Library Manager): **Adafruit ST7735 and ST7789 Library**, and **SmartRC-CC1101-Driver-Lib** (by LSatan).
3. Open `firmware/MeowMHz/MeowMHz.ino`.
4. Tools: Board **ESP32S3 Dev Module**, USB CDC On Boot **Enabled**, PSRAM **Disabled**.
5. Plug in USB-C, pick the **Port**, hit **Upload**. If it doesn't catch, replug the board as it says "Connecting…".
6. Serial Monitor at 115200 → you should see `MM ready`.

> The CC1101 can stay connected while flashing. If the sketch ever reports it
> doesn't fit, set Tools → Partition Scheme → **Huge APP**.

---

## Using it

**Controls:** 5 buttons, also mirrored over serial → `w`=up, `s`=down, `e`=ok, `b`=back, `f`=fn.

**First time? Follow [QUICKSTART](QUICKSTART.md)** — it walks you through your very
first capture step by step.

The short version:
- **Analyze** → hold your remote → it shows the frequency → **OK** locks it in.
- **Read** → **OK** to listen → press your remote → it captures and draws the waveform.
- On the captured screen, **FN** saves it to flash.
- **Transmit** → **OK** replays it. Or **Saved** → pick one → **OK** replays.

### Web UI (phone / PC)

Menu → **WEB** starts a WiFi access point. Join the `MeowMHz` network, open the IP
shown on screen (usually `http://192.168.4.1`), log in with **`MM` / `subghz`**,
and manage your saved signals from the browser. **BACK** stops WiFi.

> 📵 **Page won't load? Turn mobile data OFF.** MM's WiFi has no internet (normal —
> it's a direct link to the device), so your phone may route the browser over
> cellular instead. Turning mobile data off fixes it. Type the address with
> `http://` in front.

> ⚠️ **The web login is not real security** — the password is public (it's in this
> repo) and the access point is open. It's a convenience for transferring files;
> turn it off when you're done.

> 💾 **Reflashing firmware can wipe saved signals** (they live in internal flash).
> Back them up first by downloading the `.bin` files via the web page — that's what
> the download/import feature is for.

---

## Roadmap

**Shipped in 1.0**
- [x] Sub-GHz capture / analyze / replay
- [x] Flash-backed signal library
- [x] Web UI (send / export / import over WiFi)

**Post-1.0 (help wanted — but staying simple)**
- [ ] Enclosure
- [ ] Gentler onboarding / more beginner examples
- [ ] Signal naming / labeling

Advanced features (protocol decoding, brute-force, jamming, extra modulations,
SD storage) are intentionally **out of scope** to keep MM a clean first tool.
They're a great fit for a **fork** — go for it.

---

## Contributing

Builds, issues, and PRs welcome — see **[CONTRIBUTING.md](CONTRIBUTING.md)**.
Please keep the beginner-friendly spirit; big attack features belong in forks.
If you build a derivative, credit CyberMeow (see License).

## License

- **Code** — [MIT](LICENSE). Do what you like, including sell it; keep the copyright notice (that's the credit).
- **Docs, wiring, and artwork** — [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).

## Acknowledgments

Designed and built by **Amrio / CyberMeow**. Development done with help from
Claude (Anthropic) as a coding and design assistant.

---

## ⚠️ Legal & responsible use

Meow MHz transmits on sub-GHz frequencies, and **you are responsible for using it
legally.** Radio transmission is regulated (FCC in the US and equivalents
elsewhere) — some frequencies and power levels need a license, and some are
off-limits. Only transmit where you're permitted, and only capture or replay
signals from **devices you own or have explicit permission to test**. This is for
learning, security research, and your own equipment — not for interfering with,
accessing, or disrupting anything that isn't yours. The authors take no
responsibility for misuse.
