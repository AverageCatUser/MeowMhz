# Meow MHz — Wiring

Everything runs at **3.3V**. The display and the CC1101 sit on **two separate
SPI buses** so they never contend — this matters, don't merge them onto one bus.

## Display — ST7789 135×240 (HSPI)

| Display pin | ESP32-S3 GPIO | Notes |
|---|---|---|
| GND | GND | |
| VCC | 3V3 | **not 5V** |
| SCL / SCK | 12 | |
| SDA / MOSI | 11 | |
| RES / RST | 8 | |
| DC | 9 | |
| CS | 10 | |
| BLK | 7 | or tie straight to 3V3 |

Most 1.14" ST7789 modules are 3.3V-only (no regulator, no level shifter). If you
don't see a small SOT-23 regulator next to VCC, feed it 3V3.

## CC1101 sub-GHz radio (FSPI)

| CC1101 pin | ESP32-S3 GPIO | Notes |
|---|---|---|
| VCC | 3V3 | **not 5V** — will damage it |
| GND | GND | |
| SCK | 14 | |
| SI (MOSI) | 15 | module may label it SI or MOSI |
| SO (MISO) | 16 | module may label it SO or MISO |
| CSN (CS) | 17 | |
| GDO0 | 18 | data / interrupt line |
| GDO2 | — | leave unconnected |
| ANT | antenna | ~17.3 cm wire for 433, ~23 cm for 315 |

Pin-name translation across modules: **SI = MOSI, SO = MISO, CSN = CS.**

## Buttons

Each button is dead simple: **one leg to the GPIO, one leg to GND.** The firmware
enables internal pull-ups, so **no resistors**. All five GND legs can share one
GND pin.

| Button | GPIO | Function |
|---|---|---|
| UP | 1 | menu up / frequency up |
| DOWN | 2 | menu down / frequency down |
| OK | 4 | select / start / transmit |
| BACK | 5 | back to menu |
| FN | 6 | save (Read) / reset (Analyze) / delete (Saved) |

> On 4-pin tactile buttons, the two legs on the **same side** are internally
> connected — wire to legs on **opposite** corners, or the button does nothing.

## Flashing

- Board: **ESP32S3 Dev Module**, USB CDC On Boot **Enabled**, PSRAM **Disabled**.
- The **CC1101 can stay connected while flashing** — its pins are not boot-strapping (0/3/45/46) or USB (19/20) pins.
- To enter the bootloader, replug the board as the IDE says "Connecting…".

## Pins to avoid if you rewire

- **19, 20** — native USB (D-/D+). Leave alone.
- **0, 3, 45, 46** — strapping pins, sampled at boot.
- **33–37** — consumed by PSRAM on octal-PSRAM boards.
- **48** — onboard RGB LED on many Super Mini boards.
