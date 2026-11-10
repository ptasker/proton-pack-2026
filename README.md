# Proton Pack 2026

Firmware for a home-built Ghostbusters proton pack and neutrona wand, running on
ESP32 boards with PlatformIO and Arduino. It drives the lights, plays the sounds
from an SD card, and serves a phone-friendly control page over the pack's own
Wi-Fi.

Behaviour is modelled on
[GPStar Core Source](https://github.com/GPStarTechnologies/GPStar-Core-Source),
adapted to much simpler hardware. It is an independent implementation, not a
port: it does not run on GPStar boards and cannot talk to GPStar firmware.

## What's in the repo

| Build | Board | What it does |
| --- | --- | --- |
| `esp32dev` | ESP32-WROOM | Main pack: cyclotron, power cell, sounds, Wi-Fi page, Bluetooth speaker |
| `wand_c3` (default) | ESP32-C3 Super Mini | Wand: switches, fire button, 34 NeoPixels, 28-segment bargraph. Controls the pack over a two-wire UART link |
| `simple_pack` | ESP32-WROOM | A second, simpler pack with no separate wand board: one switch, one button, fewer pixels |

## Features

- **Four themes** (1984, 1989, Afterlife, Frozen Empire), each with its own
  sounds, cyclotron style, bargraph style and ramp timings.
- **Sound from SD card**: an 8-voice WAV mixer with loops, fades and a peak
  limiter, playing GPStar-numbered sound files through an I2S amplifier.
- **Firing and overheat**: hold to fire, staged speed-up as the 8-second limit
  approaches, then a 10-second vent sequence.
- **Wi-Fi control page**: status, theme, mute, volume, music and pack name.
- **Bluetooth speaker mode**: play phone audio through the pack.
- **Firmware updates over Wi-Fi** for the packs.

No sound files are included. You need your own SD card of GPStar-numbered WAVs
(see [docs/audio.md](docs/audio.md)).

## Hardware used

Parts are listed by type; where the code depends on a specific part, it is
named. Wiring diagrams, pin tables and power notes are in
[docs/hardware.md](docs/hardware.md).

### Main pack

| Part | Details |
| --- | --- |
| Controller | ESP32-WROOM development board |
| Cyclotron | 40-pixel WS2812 (NeoPixel) ring |
| Power cell | 15-pixel WS2812 strip |
| Amplifier | MAX98357A I2S mono amplifier breakout |
| Speaker | One small speaker, driven by the amp |
| Storage | 3.3V SPI microSD reader and a microSD card of WAV files |
| Power | 5V battery supply for the pixels and amp |

### Wand

| Part | Details |
| --- | --- |
| Controller | ESP32-C3 Super Mini |
| Pixels | 34 WS2812 pixels in one chain: 1 slo-blo, a 7-pixel vent jewel, 2 hat lights, 1 Clippard, and a 23-pixel firing strip |
| Bargraph | Barmeter BL28Z-3005SA04Y, 28 segments |
| Bargraph driver | HT16K33 I2C LED driver |
| Level shifter | BSS138 bidirectional shifter between the 3.3V C3 and the 5V driver |
| Switches | 3 maintained toggles (Activate, Vent, wand) |
| Fire button | 1 normally-open momentary button |
| Cable to pack | 3 wires: UART TX, UART RX, ground |

### Simple pack

| Part | Details |
| --- | --- |
| Controller | ESP32-WROOM on a Freenove ESP32 breakout board |
| Cyclotron | Four 7-pixel WS2812 jewels, chained |
| Power cell | 12-pixel WS2812 strip |
| Wand pixels | 10 WS2812 pixels in one chain: a 4-pixel strip, 3 single pixels, and a 3-pixel strip |
| Controls | 1 toggle switch and 1 momentary button |
| Audio | MAX98357A amplifier, speaker and SPI microSD reader, as on the main pack |

### Recommended extras

- A 330-470 ohm resistor on each pixel data line.
- A 5V-powered 74AHCT125 level shifter for the pixel data lines.
- A 500-1000 uF capacitor across the cyclotron ring's power input.
- A separate 5V supply or regulator for the ESP32, apart from the amp and
  pixels.

## Getting started

1. Install [PlatformIO](https://platformio.org/install) (the VS Code extension
   or the `pio` command line).
2. Copy `include/WifiSecrets.example.h` to `include/WifiSecrets.h` and set
   `AP_PASSWORD` (8-63 characters). The file is git-ignored. Without it the
   pack builds with Wi-Fi off.
3. Check the pins in [include/Config.h](include/Config.h) (pack) and
   [include/WandConfig.h](include/WandConfig.h) (wand) against your wiring.
4. Build and upload each board over USB, naming the port when more than one
   board is plugged in:

   ```sh
   pio device list
   pio run -e esp32dev -t upload --upload-port <pack-port>
   pio run -e wand_c3 -t upload --upload-port <wand-port>
   ```

5. Watch the boot log at 115200 baud: `pio device monitor -e esp32dev`.

The pack and wand must run matching firmware, so update both together. If `pio`
is not on your path, it is usually at `~/.platformio/penv/bin/pio`.

## Using it

| Action | How |
| --- | --- |
| Power up | Activate switch on. The pack starts up with its theme's sounds |
| Arm | Vent and wand switches on |
| Fire | Hold the fire button. Release to stop |
| Overheat | 8 seconds of firing triggers a 10-second cooldown. Release and press again afterwards |
| Mute | With Activate off, hold fire for 3 seconds |
| Bluetooth speaker | With Activate off, flip the Vent switch on and off three times within 3 seconds |
| Change theme | Web page, or USB serial keys `1`-`4` on the pack |
| Web page | Join the pack's `ProtonPack_XXXX` network and open http://192.168.4.1/ |

The fire button only fires on a fresh press: holding it through startup or
cooldown does nothing until you release and press again.

## Documentation

| Page | Covers |
| --- | --- |
| [docs/hardware.md](docs/hardware.md) | Pins, wiring and power for the pack and wand |
| [docs/wand.md](docs/wand.md) | Switch sequence, lights, bargraph, serial commands |
| [docs/pack.md](docs/pack.md) | Pack states, cyclotron and power-cell effects, serial commands |
| [docs/audio.md](docs/audio.md) | SD card format, themes, which sound plays when, mixer internals |
| [docs/wifi-bluetooth.md](docs/wifi-bluetooth.md) | Control page, HTTP API, updates over Wi-Fi, Bluetooth speaker |
| [docs/uart-link.md](docs/uart-link.md) | The wand-to-pack protocol |
| [docs/simple-pack.md](docs/simple-pack.md) | The second, simpler pack build |
| [docs/development.md](docs/development.md) | Build environments, diagnostics, tests, troubleshooting |

## Safety

- Disconnect power before changing any wiring, and never feed 5V into a GPIO.
- Power the pixels from a proper 5V supply with a shared ground, not from a
  board's 3.3V pin or USB port. The pack's LEDs alone can draw over 3A at full
  white.
- The firing strip flashes rapidly. Avoid using it around anyone sensitive to
  flashing lights.
- No smoke, heater or pump hardware is driven by this firmware.

## Tests

Host-side tests for the pack state machine and animations run without hardware:

```sh
pio test -e native
```

Five Afterlife animation tests still expect an earlier version of the effect
and currently fail; the other 17 pass. See
[docs/development.md](docs/development.md).

## Credits

- [GPStar Core Source](https://github.com/GPStarTechnologies/GPStar-Core-Source)
  is the behavioural reference for timings, sound choices and themes.
- [GBPack32](https://github.com/swbradshaw/GBPack32) by Scott Bradshaw inspired
  the original pack structure.
- Libraries: Adafruit NeoPixel, Bounce2, ESPAsyncWebServer, and Peter Sjoberg's
  [simple ht16k33 library](https://github.com/lpaseen/ht16k33).

## License

Copyright (C) 2026 Peter

This project is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. It is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See [LICENSE](LICENSE) for the full text.

GPL-3.0-or-later matches the projects this firmware builds on: GPStar Core
Source is GPL-3.0-or-later, and the wand links the GPLv3 ht16k33 library.
