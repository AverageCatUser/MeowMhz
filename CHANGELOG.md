# Changelog

All notable changes to Meow MHz. This is pre-1.0 — a working prototype, not a
finished release.

## [v7] — prototype (current)
- **Flash storage (LittleFS):** captures now save to internal flash and survive power-off.
- **Saved screen:** browse a library of stored signals, replay one directly, or delete it.
- FN on the Read screen now saves the current capture.

## [v6] — prototype
- Removed all software bootloader escape hatches (reboot screen, FN-at-boot, button combo, serial trigger) — flashing is now done by replugging.

## [v5] — prototype
- Analyzer RSSI settle time increased for more reliable readings.
- Header frequency refreshes immediately when locking a frequency in Analyze.

## [v4] — prototype
- **Analyze:** RSSI-hopping frequency finder — hold a remote, it reports the frequency.
- Expanded to 14 scannable/tunable frequencies.
- Fixed menu navigation to handle more than two items.

## [v3] — prototype
- **Cat companion:** animated mascot on the menu that reacts to state.
- Moved display and CC1101 to separate SPI buses (no contention).
- Waveform-redraw fixes.

## [v2] — prototype
- Test menu UI: top bar, menu list, icon panel, serial control.
- Black/green phosphor theme.

## [v1] — prototype
- First bring-up: ST7789 display working on ESP32-S3.

---

### Toward 1.0
- Battery + charging
- Enclosure
- Antenna tuning guidance
- Complete build documentation
