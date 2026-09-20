# Changelog

All notable changes to Meow MHz.

## [1.0] — first release 🎉
The first stable release. Sub-GHz capture, analyze, replay, a flash-backed
signal library, and a phone/PC web interface — all on ~$12 of hardware.

- **Web UI (AP mode):** toggle a WiFi access point from the menu; connect a phone or PC and log in (`MM` / `subghz`) to a plain-text page that lists saved signals, sends them, downloads them to your device, and imports `.bin` files back.
- **Security fixes:** upload endpoint now requires login before writing to flash; web routes register once instead of on every toggle.
- Boot splash shows `v1.0`.

## [v7] — prototype
- **Flash storage (LittleFS):** captures save to internal flash and survive power-off.
- **Saved screen:** browse stored signals, replay one directly, or delete it.

## [v6] — prototype
- Removed software bootloader escape hatches — flashing is done by replugging.

## [v5] — prototype
- Analyzer RSSI settle time increased; header frequency refreshes when locking a frequency.

## [v4] — prototype
- **Analyze:** RSSI-hopping frequency finder. Expanded to 14 frequencies. Menu handles >2 items.

## [v3] — prototype
- **Cat companion.** Separate SPI buses for display and CC1101. Waveform-redraw fixes.

## [v2] — prototype
- Test menu UI, black/green phosphor theme.

## [v1] — prototype
- First bring-up: ST7789 display on ESP32-S3.

---

### Post-1.0 ideas (help wanted)
- Battery + charging refinement, enclosure
- Signal naming / labeling
- Protocol decoding (identify Princeton / CAME / etc.)
- SD card storage option
- AP password + per-device web password (harden the web UI)
