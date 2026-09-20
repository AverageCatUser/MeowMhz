# Quickstart — Your First Capture 🐱

Never done this before? Perfect. This is written for you. In about five minutes
you'll capture a signal from a remote and play it back. No RF knowledge needed.

## What you need

- A built Meow MHz (see the [README](README.md) for parts and wiring).
- **A simple remote to practice on** — the best first targets are things with
  cheap fixed-code remotes:
  - a ceiling-fan remote
  - a wireless doorbell button
  - an old garage remote
  - a cheap remote power outlet
- **Something you own.** Only ever capture from your own devices.

> ⚠️ Don't practice on car keys or modern garage openers — those use *rolling
> codes* that change every press, so a replay does nothing. That's not a bug;
> no tool can replay those. Stick to simple stuff for learning.

## Step 1 — Power on and check the radio

Turn it on. On the splash screen, look for **`CC1101: OK`**.

- **`OK`** → your radio is wired right, keep going.
- **`FAIL`** → the CC1101 isn't talking. Re-check its wiring and solder joints
  (see the README) before continuing. Everything else will work but you won't be
  able to capture.

## Step 2 — Find the frequency (Analyze)

You need to know what frequency your remote uses. MM figures it out for you.

1. From the menu, select **ANALYZE**.
2. **Press and hold your remote's button** near the device.
3. Watch the bars — one will jump up. The readout shows a frequency like
   **`433.92 MHz`** (fans and doorbells are usually 315 or 433.92).
4. Press **OK** to lock that frequency in. Press **BACK**.

Nothing jumping? Hold the remote right against the board (your antenna may be
short), and make sure you're actually holding the button down during the scan.

## Step 3 — Capture it (Read)

1. From the menu, select **READ**.
2. Press **OK** — the screen says *listening…*
3. **Press your remote's button.**
4. It captures automatically and draws the signal as a green waveform, with an
   edge count. A clean, repeating shape means you caught a real signal. 🎉
5. Press **FN** to **save** it to flash so it's kept.

Got garbage or nothing? You're probably on the wrong frequency — go back to
**ANALYZE** and try again.

## Step 4 — Play it back (Transmit)

1. Press **BACK** to the menu, select **TRANSMIT**.
2. Press **OK** — it sends the signal 5 times.
3. Watch your device react. If it's a fan, it should spin. A doorbell should
   ring. That's you replaying a real RF signal. 🐱📡

Didn't trigger? Some receivers are picky — try **TRANSMIT → OK** again, or make
sure you captured on the right frequency.

## Step 5 — See it on your phone (optional)

1. Menu → **WEB**. The screen shows a WiFi name and an address.
2. On your phone: join the **`MeowMHz`** WiFi. **Turn mobile data OFF** (or the
   page won't load).
3. Open the address shown (like `http://192.168.4.1`) and log in: **`MM`** /
   **`subghz`**.
4. You'll see your saved signals — you can replay them, download them to your
   phone, or import ones back.
5. Press **BACK** on the device to turn WiFi off.

## That's it

You've found a frequency, captured a signal, saved it, replayed it, and pulled it
onto your phone. That's the whole core of sub-GHz work — everything else is just
variations on this loop.

Stuck on anything? Open an [issue](../../issues) and say what happened — beginner
questions are welcome.
