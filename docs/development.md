# Development

## Build environments

| Environment | Board | Purpose |
| --- | --- | --- |
| `wand_c3` (default) | ESP32-C3 Super Mini | Wand firmware |
| `esp32dev` | ESP32-WROOM | Main pack firmware |
| `pack_ota` | ESP32-WROOM | Same as `esp32dev`, uploaded over the pack's Wi-Fi |
| `simple_pack` | ESP32-WROOM | Simple pack firmware (MAC-checked upload) |
| `simple_pack_ota` | ESP32-WROOM | Simple pack, uploaded over its Wi-Fi |
| `bargraph_scan_c3` | ESP32-C3 | Diagnostic: I2C scan for the bargraph driver |
| `pack_audio_test` | ESP32-WROOM | Diagnostic: bare I2S tone, no pack logic |
| `simple_pixel_test` | ESP32-WROOM | Diagnostic: walks each simple-pack pixel chain |
| `native` | Host | Unit tests |

Each environment compiles exactly one file from `src/`. Most of the code is in
header files under `include/`.

```sh
pio run -e <env>                               # build
pio run -e <env> -t upload --upload-port <p>   # upload
pio device monitor -e <env>                    # serial log, 115200 baud
```

Tips:

- With several boards connected, run `pio device list` and name the port.
  Uploading the wrong build to a board replaces its firmware.
- Close the serial monitor before uploading.
- On the C3, keep the fire button released during reset and upload. If a first
  upload will not connect, hold BOOT, tap RESET, release BOOT and retry.
- On the ESP32-WROOM, if an upload sticks at "Connecting", hold BOOT until it
  starts.
- Mute or unpower the amplifier before uploading to the pack.
- A diagnostic build replaces the normal firmware. Upload the normal
  environment again when you are done.

## Diagnostic builds

**`bargraph_scan_c3`** scans I2C on GPIO4/5 at 100 kHz and reports any device
found; the bargraph driver should answer at `0x70`. Send `?` to scan again. A
reply proves the driver is reachable, not that the display is wired correctly:
use `BARGRAPH_DIAGNOSTIC_MODE` in the wand firmware for that (see
[wand.md](wand.md)).

**`pack_audio_test`** generates samples directly in `loop()`, with no audio
task, SD card, wand link or animations, to separate amplifier and power
problems from firmware ones. It lights the power cell steady blue and boots
silent.

| Key | Action |
| --- | --- |
| `t` | Start a **continuous** 1 kHz tone at about -12 dBFS. It does not stop on its own |
| `m` | Back to silence |
| `l` | Resend the static lights (rejected while the tone plays) |
| `?` | Status |

This tone is much louder than the normal firmware's test tone. Keep the speaker
away from your ears.

**`simple_pixel_test`** blinks each simple-pack chain's GPIO three times to
identify it, then for each chain walks a white dot along every pixel, shows red,
green, blue and white, and lights each group in its own colour.

## Tests

```sh
pio test -e native
```

[test/test_pack/test_main.cpp](../test/test_pack/test_main.cpp) covers the pack
state machine (boot modes, start-up, firing, cooldown, shutdown, held-button
guards, `millis()` rollover) and the cyclotron animations. The tests run on the
host and do not check real LED output, wiring or switches.

**Known failures:** five Afterlife animation tests still expect the earlier
fixed-head effect and its timings, and fail against the current per-LED fade
renderer. The other 17 pass. There are no tests for the wand, audio or web
code.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| No LEDs | Supply voltage, shared ground, the right GPIO, and that data goes to DIN, not DOUT |
| Flicker or random colours | Power connections; use a level shifter and a short data wire |
| Red and green swapped | Change `NEO_GRB` to `NEO_RGB` |
| RGBW pixels | Change `NEO_GRB` to the pixel's four-channel order, usually `NEO_GRBW` |
| Afterlife lights miss the lenses | Adjust `AFTERLIFE_WINDOW_OFFSET` and `AFTERLIFE_WINDOW_PIXELS` |
| Bargraph found on I2C but dark | The HT16K33 and the level shifter's HV side need 5V |
| Pack never starts | It waits for the wand. Check the UART wires, and that both boards run matching firmware |
| Wi-Fi is off | `include/WifiSecrets.h` is missing; see [wifi-bluetooth.md](wifi-bluetooth.md) |
| Resets while firing | A power dip. See the power notes in [hardware.md](hardware.md) |
| Static or ticks in the audio | Check the log for underruns; clean `._` files off the card (see [audio.md](audio.md)) |

## Upstream reference

New behaviour is checked against
[GPStar Core Source](https://github.com/GPStarTechnologies/GPStar-Core-Source)
(GPL-3.0-or-later) first. This project adapts GPStar's behaviour and timings to
different hardware; it does not compile GPStar source. If you copy source from
it, keep its attribution and comply with its license.

`.gitignore` excludes a local `reference/` folder, which is a convenient place
to keep a checkout of GPStar for reading.

`.github/copilot-instructions.md` holds the working notes used when developing
this project with an AI assistant, including bring-up history for this
particular build.
