# Contributing to Meow MHz

Thanks for wanting to build on this. It's a small solo project, so anything helps.

## Ways to contribute

- **Build one and report back** — tell us which parts you used, what worked, what didn't. Real-world builds are the most useful thing.
- **File issues** — bugs, unclear docs, a remote it couldn't capture. Include your board, the frequency, and what the waveform looked like.
- **Pull requests** — bug fixes, cleaner code, better docs, new features.
- **Forks** — building a bigger firmware (WiFi, BLE, IR, protocol decoding) on top of this base is welcome and encouraged. This repo stays focused on clean sub-GHz.

## Guidelines

- Keep the firmware **one `.ino` file** for now — it's easier for people to grab and flash. If a feature genuinely needs splitting, open an issue to discuss first.
- Match the existing style: section-header comment blocks, minimal formatting, comments where the *why* isn't obvious.
- Test on real hardware before submitting — note which board and modules you tested on.
- If your change touches wiring or pins, update `docs/WIRING.md` too.

## Credit

If you ship a derivative firmware, please credit **CyberMeow** — that's the one
condition of the MIT license, and it's how the project stays connected to its
source.

## Responsible use

This tool transmits RF. Contributions that add capabilities should keep the
[legal & responsible-use notice](README.md#️-legal--responsible-use) intact.
Don't add features whose only purpose is to interfere with or access equipment
that isn't the user's own.
