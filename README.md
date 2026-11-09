# Proton Pack 2026

Wand development now targets an **ESP32-C3 Super Mini**, using
[GPStar Core Source](https://github.com/GPStarTechnologies/GPStar-Core-Source)
as the upstream reference. The ESP32-WROOM pack retains its existing lighting
effects and now follows the wand over UART. The default PlatformIO environment is `wand_c3`; `esp32dev`
still builds only the pack. Neither environment compiles the other's entry point.

## Wand Hardware

Settings are in [include/WandConfig.h](include/WandConfig.h).
Current defaults are `LED_DIAGNOSTIC_MODE = false` and
`BENCH_IDLE_FIRING_MODE = false`: all three physical switches and the GPIO2
fire button are active following the switch rewiring.

The table below matches the configured pins. Both missing-switch bypass options
are disabled.

| Connection       | C3 GPIO | Role                                               |
| ---------------- | ------- | -------------------------------------------------- |
| Switch 1         | 0       | Activate / wand power                              |
| Switch 2         | 1       | Vent light and firing gate                         |
| Switch 3         | 20      | Wand enable / Clippard                             |
| Momentary button | 2       | Fire                                               |
| NeoPixel DIN     | 21      | One daisy chain, 34 pixels; RGB/GRB format assumed |
| Bargraph SDA     | 4       | BSS138 LV1/HV1 to HT16K33 SDA                       |
| Bargraph SCL     | 5       | BSS138 LV2/HV2 to HT16K33 SCL                       |
| Pack UART RX     | 6       | Green wire from main ESP32 GPIO25 TX               |
| Pack UART TX     | 7       | Blue wire to main ESP32 GPIO35 RX                  |

The LED order is zero-based and follows the physical data chain:

| Section      | Pixel indices | Count |
| ------------ | ------------- | ----- |
| Slo-blo      | 0             | 1     |
| Vent jewel   | 1-7           | 7     |
| Hat          | 8             | 1     |
| Second hat   | 9             | 1     |
| Clippard     | 10            | 1     |
| Firing strip | 11-33         | 23    |

All switches are assumed to be maintained toggles connected between their GPIO
and GND; the firing button is normally-open and momentary. Inputs use
`INPUT_PULLUP`, so LOW means on. Confirm this wiring before uploading and never
connect 5V to a GPIO. GPIO2 is a C3 strapping pin: leave the firing button released
during reset/upload. If it interferes with startup, move it to a suitable unused
non-strapping GPIO and update the configuration. Keep GPIO18/19 reserved for USB.

Pixels are assumed to be RGB WS2812-compatible, GRB order, 800 kHz. Power them
from a suitable 5V supply with common ground, not from the C3's 3.3V pin. Use a
330-470 ohm data resistor near DIN and preferably a 5V-powered 74AHCT125 level
shifter. Disconnect power before rewiring. When the C3 is USB-powered, do not
also feed an external supply into its power pins. The LED supply rating is not
yet confirmed; 34 RGB pixels can require about 2A at full white. Normal brightness
is now 255/255: verify the supply and wiring before uploading, with headroom
(for example, a regulated 5V 3A supply for the LEDs). Do not feed this full-brightness
chain through the C3's USB/5V pin. Software brightness is not a substitute for
adequate power, wiring, and circuit protection.

## Wand Bargraph

The normal `wand_c3` firmware includes nonblocking bargraph animations in
[include/WandBargraph.h](include/WandBargraph.h), styled by the pack's system
theme (adapted from GPStar's 28-segment `bargraphRamp()`, `bargraphPowerCheck()`
and `bargraphRampFiring()` timings):

- **1984 / 1989 (GPStar Super Hero mode):** activation sweeps up and back down at
  24 ms per segment, then idle repeats a bottom-to-top-to-bottom sweep at 12 ms per
  segment with 90 ms pauses at each end.
- **Afterlife / Frozen Empire (GPStar Original mode):** activation fills from the
  bottom at 64 ms per segment (about 1.8 s), then holds all 28 segments.
- **Firing (all themes):** GPStar's Super Hero paired lines from the center to
  both ends and back, 15 ms per step, speeding to 13/11/9/7 ms when less than
  1/3, 1/4, 1/5 and 1/6 of the 8-second firing limit remains.
- **Overheat:** the cooldown starts with a top-down drain at 80 ms per segment;
  when it ends, the theme's activation ramp replays. Activate off also drains
  top-down at 80 ms per segment.

The pack sends its theme to the wand in a `PackStatus` frame (type 3) on connect,
on change, and every 500 ms. The wand saves the last theme it received
(Preferences namespace `wand`, key `theme`; default Afterlife) and prints
`Pack theme: ...` when it changes. The project has no rotary encoder, so the
bargraph always shows the fixed maximum power level.

`BARGRAPH_DIAGNOSTIC_MODE = true` replaces those animations with the original
segment check alongside the existing wand lights and controls. Turn Activate
on to light one segment at a time, stepping through all 28 every 250 ms,
followed by a one-second blank pause. Activate off clears the bargraph, and
reactivation starts again at the first segment.

The display is the confirmed Barmeter **BL28Z-3005SA04Y Y80**. Its 11 connections
follow [GPStar's bargraph wiring table](reference/GPStar-Core-Source/docs/BARGRAPH.md#L46):
driver A0-A6 connect to display pins 22, 1, 19, 18, 7, 10, 11; driver C0-C3 connect
to pins 21, 15, 13, 16. The other legs remain unused. The segment RAM address is
`segment / 4 + 16 * (segment % 4)` for zero-based segment numbers 0-27.

I2C uses GPIO4/5, address `0x70`, and 100 kHz as in the successful scanner check.
`BARGRAPH_BRIGHTNESS = 1` is a dim HT16K33 setting (2/16 PWM duty), independent of
NeoPixel brightness. Display RAM is blanked before initialization. Updates send
only changed frames, without animation delays. Missing or failed I2C disables
the bargraph update path until reset without disabling the wand controls;
startup/monitor-connection messages report bargraph status.

The integrated sweep was confirmed working on 2026-09-24 after both the BSS138
HV reference and HT16K33 VDD were connected to the external regulated 5V
supply. An I2C acknowledgement alone does not prove that the display driver has
its required operating power. Keep all grounds common, and do not connect the
external 5V rail to a USB-powered C3's power pins.

## Bargraph I2C Check

The temporary `bargraph_scan_c3` target builds only
[src/bargraph_scan.cpp](src/bargraph_scan.cpp). It scans I2C on GPIO4 (SDA) and
GPIO5 (SCL) at 100 kHz, clears the NeoPixel chain, and ignores all switches and
the fire button. The normal `wand_c3` and pack targets are unchanged.

Connect C3 3.3V to the BSS138 LV reference, and the regulated 5V bus to its HV
reference and HT16K33 VDD. GPIO4 connects through LV1/HV1 to SDA; GPIO5 connects
through LV2/HV2 to SCL. All three boards must share ground. Keep the C3 side at
3.3V and the HT16K33 side at 5V; do not connect external 5V to a USB-powered C3's
power pins. Disconnect power before changing connections.

With only the C3 connected, upload and inspect the scanner using:

```sh
~/.platformio/penv/bin/pio run -e bargraph_scan_c3 -t upload
~/.platformio/penv/bin/pio device monitor -e bargraph_scan_c3 --baud 115200
```

Send `?` to repeat the scan. When multiple boards are connected, identify the
C3's current port with `pio device list` and explicitly select that port for
upload/monitor. Keep fire released during reset/upload and close the monitor
before uploading.

On 2026-09-24, the scanner was built, uploaded, and repeatedly reported
`Found I2C device at 0x70`. This confirms communication with the expected driver
address, not the LED segment wiring. GPStar also probes `0x70` before initializing
its bargraph; this diagnostic uses 100 kHz rather than its 400 kHz bus setup.
This scanner sends no segment-driving commands. Use the integrated
[segment check](#bargraph-segment-check) above for visual wiring verification.

The scanner temporarily replaces the running wand firmware. To restore normal
operation, upload `wand_c3` to the same identified C3 again. The project's default
environment remains `wand_c3`.

## Optional Idle/Firing Bench

For button-only operation, set `LED_DIAGNOSTIC_MODE = false` and
`BENCH_IDLE_FIRING_MODE = true` in [include/WandConfig.h](include/WandConfig.h)
and upload again. In this optional mode, all three switches are assumed on
without initializing or polling their pins.
The physical GPIO2 button remains active LOW to GND and debounced for 25 ms.

- **Idle:** starts immediately, with the slo-blo, vent jewel, both hats, and
  Clippard illuminated at brightness 255/255. The firing strip stays dark.
- **Firing:** press and hold for alternating red/blue stream pulses, 32 ms on
  then 32 ms dark. Contrasting 2-6-pixel bursts step every 8 ms, with firing
  frames sent every 4 ms; idle and diagnostic frames remain at 20 ms. The folded
  strip's 12th LED is the physical tip: strip index 11, whole-chain index 22
  (`FIRING_TIP_PIXEL`). It flashes white for 16 ms every 64 ms, with the stream
  on the other 22 pixels. Release to return to idle; the body indicators remain
  steady throughout firing.

The bursts and tip flash adapt GPStar's moving multi-pixel stream and optional
tip strobe to this single folded strip. The stronger alternating colors, dark
gaps, and timing are a local preset, not an exact upstream port. Body animation
still follows electrical chain order.
GPStar's Afterlife/Frozen Empire hats normally stay lit, but flash during an
overheat warning; the Clippard follows the wand switch. Its vent light goes to
full brightness during normal proton firing rather than strobing. This bench
keeps the existing indicator colors and does not add overheat-warning flashes.
The strip and tip use rapid flashing light; avoid viewing them directly or using them
around anyone sensitive to flashing lights.

Holding the button at startup cannot fire; release it before making a new press.
The existing 8-second continuous firing limit remains. At that limit, the wand
returns to the idle lighting for a 10-second overheat lockout (GPStar's level 5
overheat length), then requires a new press.
Presses made during the lockout are not queued. There is no startup fade or
separate cooling light effect in this mode.

This retains GPStar's hold-to-fire/release-to-idle behavior while deliberately
bypassing the switch gates for bench work. Serial reports `Idle`/`Firing`; `?`
shows effective inputs, with the three simulated switches always reading 1.
Physical switches and serial switch commands cannot turn this mode off.

To return to the current default, set both mode flags to `false` and retain the
[physical-switch configuration](#wand-switch-sequence) below before uploading.
No audio or smoke is added. When enabled, the UART link also reports bench
mode's assumed-on switches and firing/cooldown state to the pack.

## LED-Only Bring-Up

To repeat the all-pixel diagnostic, set `LED_DIAGNOSTIC_MODE = true` in
[include/WandConfig.h](include/WandConfig.h) and upload again. This overrides
idle/firing bench mode. It starts automatically on GPIO21,
without initializing or reading any switches or the button, and without waiting
for a serial monitor. The repeating sequence is:

1. One dim white pixel travels from index 0 through 33, spending 200 ms on each.
2. The whole chain shows red, green, blue, then white for one second each.
3. All pixels go dark for one second before the sequence repeats.

Diagnostic brightness is 16/255, separate from normal wand brightness 255/255.
This deliberately bypasses GPStar's switch gates and section-specific effects
to check the physical chain. Switches do not stop the diagnostic; disconnect LED
power to stop it. GPIO2 is still a hardware strapping pin even when ignored by
the firmware, so keep fire released during reset/upload.

Set `LED_DIAGNOSTIC_MODE = false` and upload again to restore button control.
Keep `BENCH_IDLE_FIRING_MODE = false` to also enable physical switch control.
RGB/GRB remains an assumption, not a confirmed identification of the LED modules;
RGBW or mixed-format modules require a matching data format.

## Wand Switch Sequence

This is the current default after rewiring: Activate is GPIO0, Vent is GPIO1,
and wand enable is GPIO20. Both missing-switch bypass options and both mode
flags are `false`.
The behavior follows GPStar's **Super Hero** staged activation, not
its Original mode's all-switches-required startup. Both hats retain their
Afterlife-style lighting; the firing preset remains fixed without an encoder.

1. Activate on GPIO0 (left control-box toggle) starts a 1.5-second light fade-in.
  Slo-blo and both hats illuminate; the Clippard follows switch 3. Activate off
  immediately clears every LED after input debounce.
2. Vent on GPIO1 (top right toggle) adds the orange vent jewel. Turning it off
  extinguishes the vent and stops firing, while the other powered indicators
  remain on.
3. Wand enable on GPIO20 (lower right toggle) controls the Clippard and the other
  firing gate. Turning it off extinguishes the Clippard and stops firing.

Current build: the Vent switch stopped responding after shell installation, so
[include/WandConfig.h](include/WandConfig.h) sets `VENT_SWITCH_PIN = -1` and
`VENT_FOLLOWS_WAND_SWITCH = true`. The lower-right switch drives both Vent and
wand enable: vent jewel, Clippard, the pack's Vent/arm sounds, firing, and the
Bluetooth mode gesture. Set the pin back to 1 and the flag to false once the
switch wiring is fixed.
   `BENCH_ENABLE_WITHOUT_WAND_SWITCH = false`; a physical switch always takes
   precedence over that setting when a GPIO is assigned.
4. With startup complete and all gates enabled, a new button press fires only
   the final 23 pixels. Releasing stops the firing strip. Holding for 8 seconds
   starts the 10-second overheat cooldown; release and press again afterward.

All inputs are active LOW to GND with 25 ms debounce. Maintained switch positions
are read at startup. Unlike the optional two-state bench mode, normal cooldown
blinks the indicators. Turning Activate off interrupts any state. Turning a
firing gate back on while holding fire does not restart firing; release and
press again once ready. The barrel sensor and audio remain deferred.

Both hats use the upstream Afterlife-style powered-on behavior. Indicator colors
are configurable; the initial choices are warm red slo-blo, orange vent, white
hats, and full-white Clippard (RGB 255/255/255 for maximum output through its
diffuser). The firing strip uses the red/blue bursts and mapped
white tip strobe described above.
Holding fire through startup, rearming, or cooldown does not automatically fire.
This fresh-press guard and the fixed animation/state timings are deliberate bench
adaptations, not a frame-for-frame recreation of all GPStar effects.

In normal mode, at 115200 baud, serial reports configuration and state changes.
`?` prints inputs and help. Serial controls (`p`, `v`, `w`, `f`, `s`) only apply
when the corresponding hardware pin is `-1`, so they cannot override any of the
installed switches/button. No audio, smoke, vibration,
barrel-extension sensor, or network is initialized. The bargraph runs the
animations described above, and the UART link reports wand state to the pack.

## Wand-to-Pack UART Link

Both firmware targets must be updated for this link to work. It is enabled by
`PACK_LINK_ENABLED` in [include/WandConfig.h](include/WandConfig.h) and
`WAND_LINK_ENABLED` in [include/Config.h](include/Config.h).

| Wire | C3 wand | Main ESP32 pack |
| ---- | ------- | --------------- |
| Green | GPIO6 RX | GPIO25 TX |
| Blue | GPIO7 TX | GPIO35 RX |
| Ground | GND | GND |

Both boards use 3.3V UART logic; do not route these signals through the bargraph's
5V level shifter. GPIO34/35 on the main ESP32 are input-only, so GPIO35 is valid
for RX but GPIO34 cannot replace GPIO25 TX. Disconnect power before rewiring.
Share ground, but do not join the boards' USB 5V supplies together.

The C3 sends Activate, Vent, wand-enable, physical fire-button state, the
controller's startup/firing/cooldown flags, and a firing speed level derived
from its remaining overheat time. The pack follows these rules:

- Boots off and waits for valid wand data; its standalone auto-start is ignored.
- Activate on starts the pack startup animation and sounds. Activate off stops
  firing and runs the 11.8-second GPStar Afterlife shutdown (10.5 s slowdown,
  1.3 s fade) with its sound.
- Accepted wand firing uses GPStar-style staged acceleration of the cyclotron
  and power cell as overheat approaches, rather than an immediate fixed firing
  preset. Release or either firing gate off returns to idle speed. Vent switch
  on alone is a firing gate, not a command to start cooldown.
- Wand cooldown requests the pack's 10-second dimmed overheat cooldown, with
  GPStar's overheat sounds.
  The pack also retains its own independent eight-second firing limit.
- No valid, new status for 500 ms stops firing and initiates shutdown. Bad CRC,
  wrong version, or duplicate sequence packets do not refresh that timeout.
- Startup, reconnect, and cooldown require ready gates, a received released
  button state while the pack is idle, and a subsequent firing edge. Held or
  lockout presses are not queued. If the pack restarts while the wand is on,
  wait for pack startup, release fire, and press again.
- With link mode enabled, pack USB `p/f/s/v` controls are rejected explicitly.
  USB `1/2` cyclotron selection and `?` help remain available. Physical pack
  power/fire/vent inputs must remain unassigned to avoid competing authorities.

The transport uses UART1 on each board at 9600 baud, 8N1. Eight-byte packets
contain `A5 5A`, version 2, type (status=1, acknowledgement=2), an 8-bit sequence,
flags, speed level (1-4), and CRC-8 (polynomial `0x07`, initial value zero, over
the first seven bytes). Non-firing packets must use speed level 1.
Version 1 firmware is incompatible: update both boards together.
Flag bits 0-6 are Activate, Vent, wand-enable, fire-held, starting, firing, and
cooling; bit 7 is reserved. State or speed-level changes send at most once per
20 ms and unchanged state repeats every 100 ms. Partial frames expire after 50 ms; parser work is
bounded and errors are reported with rate limiting.

The pack echoes sequence/flags/speed level as a receipt acknowledgement over the green wire.
This is not a firing acceptance message: the pack can reject firing for its own
safety interlocks. Missing acknowledgements are reported on the C3 without
disabling its standalone lights. If only the green wire fails, the pack can still
follow incoming C3 state; pack shutdown is triggered by loss of the blue-wire
status stream. Boot/status and connection-change messages report link health.
The NeoPixel-only diagnostic sends an off status so it cannot activate the pack.

This is a limited behavioral adaptation of GPStar's power/firing/overheat and
heartbeat handling, not a source import or GPStar-compatible serial protocol.
GPStar uses SerialTransfer and a larger command/synchronization system; do not
connect one of these firmware targets to stock GPStar firmware expecting it to
understand these packets. No dependencies or reference source imports were added.

Set the pack's `WAND_LINK_ENABLED = false` to restore its original standalone
auto-start, physical inputs, and USB power/fire/vent controls. Set the wand's
`PACK_LINK_ENABLED = false` to stop its UART traffic. Neither option changes the
wand's physical switch or fresh-press behavior.

Required before hardware validation:

```sh
~/.platformio/penv/bin/pio run -e wand_c3 -e esp32dev
~/.platformio/penv/bin/pio device list
~/.platformio/penv/bin/pio run -e esp32dev -t upload --upload-port <current-pack-port>
~/.platformio/penv/bin/pio run -e wand_c3 -t upload --upload-port <current-C3-port>
```

Identify the C3 by USB `303A:1001`, serial `44:B1:76:19:7B:A4`, and the currently
used pack adapter by `1A86:7523`. Ports can change; select them explicitly.
Keep Activate off and fire released during uploads. After both uploads, verify
startup, firing/release, each gate, cooldown, and loss/reconnection of the link.
No smoke, audio, or other outputs are controlled by this link.

## Build and Upload the Wand

The C3 Super Mini uses PlatformIO's `esp32-c3-devkitm-1` profile with DIO flash
and native USB serial enabled. Firmware serial does not use GPIO21, which is
reserved for the LED data chain. This assumes the usual 4MB-flash Super Mini.

Build only, without changing the connected device:

```sh
~/.platformio/penv/bin/pio run --environment wand_c3
```

To install it on the C3, replacing its current firmware:

```sh
~/.platformio/penv/bin/pio run --environment wand_c3 --target upload
```

Optional serial inspection:

```sh
~/.platformio/penv/bin/pio device monitor --environment wand_c3 --baud 115200
```

If multiple boards are connected, use `~/.platformio/penv/bin/pio device list`
and specify the C3's port with `--upload-port <port>` or monitor `--port <port>`.
Do not reuse the old pack's serial port without checking. Close the monitor
before uploading. For first-flash connection trouble, hold BOOT, tap RESET,
release BOOT, and retry the upload; tap RESET afterward if needed.

Builds, uploads, and hardware checks are user-run. No wand test suite is added
for this bring-up stage; the existing pack tests remain unchanged.

## Wand Code and Upstream

- [include/WandConfig.h](include/WandConfig.h): pins, chain sections, colors, and timing.
- [include/WandController.h](include/WandController.h): standalone states and firing gates.
- [include/WandInputs.h](include/WandInputs.h): debounced switches and optional serial inputs.
- [include/WandLights.h](include/WandLights.h): section-aware NeoPixel output.
- [include/WandBargraph.h](include/WandBargraph.h): dim HT16K33 segment sweep alongside normal wand operation.
- [include/WandPackLink.h](include/WandPackLink.h): shared bounded UART framing and CRC.
- [include/WandPackSender.h](include/WandPackSender.h): wand snapshots and acknowledgement diagnostics.
- [include/PackWandReceiver.h](include/PackWandReceiver.h): pack state control and link-loss interlocks.
- [src/wand.cpp](src/wand.cpp): C3 setup, loop, and USB diagnostics.

The Git-ignored local reference is
[reference/GPStar-Core-Source/README.md](reference/GPStar-Core-Source/README.md),
at revision `1a5601b` (V6.2.2). Reviewed behavior includes the NeutronaWand
standalone pack-handshake bypass, Super Hero switch gates, vent/Clippard control,
Afterlife hats, and pack serial power/firing/overheat and disconnect handling.
Its native wand targets are AVR Mega and ESP32-S3, not C3.
This is a first lights-only C3 adaptation of those behaviors, not a full GPStar
firmware port; no GPStar application source is compiled or imported into this firmware.
GPStar is GPL-3.0-or-later: future source imports must preserve attribution and
comply with that license. Keep the reference checkout out of project builds and
version control, and refresh it only when explicitly requested.

The C3 now links Peter Sjoberg's
[simple ht16k33 library](https://github.com/lpaseen/ht16k33), the GPLv3 dependency
also used by GPStar. Preserve its license and notices; distributing linked
firmware requires complying with GPLv3, including providing corresponding source.

The older GBPack32 reference was also checked locally. Its pack-driven wand
architecture and original ESP32 pin map are not carried into the C3 target.
The limited UART link above connects the existing controllers. Main-pack I2S
silence/test-tone bring-up is available below; SD playback, audio mixing, and a
broader migration of the main ESP32 to GPStar behavior remain later work.

## ESP32 Pack (Legacy Lighting)

Lights-first proton pack firmware for a classic ESP32-WROOM development board,
built with PlatformIO and Arduino. The default configuration runs an Afterlife
cyclotron effect on a 40-pixel ring at GPIO 22 and a 15-pixel blue power cell at
GPIO 23. The 1984 cyclotron remains selectable. No buttons, audio, smoke,
display, or SD card are required.

The preserved pack implementation takes architectural inspiration from
[Scott Bradshaw's GBPack32](https://github.com/swbradshaw/GBPack32/tree/main/source/ProtonPack):
separate pack state, input, and output components. This is an independent
implementation, not a fork or source import. An explicit upstream license was
not identified during the initial review; confirm permission before importing
its source or any third-party audio assets.

## Legacy Pack Hardware

| Component                            | Configuration                                                   |
| ------------------------------------ | --------------------------------------------------------------- |
| Cyclotron                            | 40 RGB NeoPixels, GPIO 22, brightness 255                       |
| Power cell                           | 15 RGB NeoPixels, GPIO 23, brightness 32; output enabled        |
| Pack power                           | Optional maintained toggle or momentary button; pin unassigned  |
| Fire                                 | Optional momentary button; pin unassigned                       |
| Vent                                 | Optional momentary button; lights-only cooldown; pin unassigned |
| Audio | MAX98357A I2S silence/manual test tone only; BCLK26, LRCLK27, DIN14 |
| SD playback, smoke, wand LEDs, display | Not implemented or initialized                               |

LEDs are assumed to be 5V WS2812/WS2812B RGB pixels with GRB color order and
800 kHz data, not RGBW pixels. The ring and power cell are enabled. Optional
switch/button pins remain `-1`, which disables their hardware access. The wand
link controls pack power, firing, and cooldown by default. No smoke
relay, heater, or pump output is driven.

## Legacy Pack Code

- [include/Config.h](include/Config.h): hardware assignments, LED counts, brightness, and timing.
- [include/PackController.h](include/PackController.h): hardware-independent pack states and transitions.
- [include/PackInputs.h](include/PackInputs.h): debounced switches and serial test commands.
- [include/Cyclotron.h](include/Cyclotron.h): retained 1984 pattern and separate Afterlife pattern.
- [include/PackLights.h](include/PackLights.h): mode selection, LED rendering, and blue power bar.
- [include/PackAudio.h](include/PackAudio.h): continuous I2S silence and manual low-level tone.
- [src/main.cpp](src/main.cpp): initialization and the cooperative update loop.
- [test/test_pack/test_main.cpp](test/test_pack/test_main.cpp): host-side pack-state and animation tests.

Adafruit NeoPixel drives the LEDs and Bounce2 debounces the switches. PlatformIO
installs these dependencies. Unity is used only for the native tests.

## Legacy Pack Wiring

Disconnect power before changing wiring. Check the ring's rated voltage before
connecting its supply.

| Connection                            | Destination                                     |
| ------------------------------------- | ----------------------------------------------- |
| External regulated 5V supply positive | Ring 5V / V+                                    |
| External supply negative              | Ring GND and ESP32 GND                          |
| ESP32 GPIO 22 (IO22)                  | Ring DIN, through a 330-470 ohm series resistor |
| ESP32 USB                             | Computer, for board power and uploading         |

GPIO 22 means the pin labeled 22 or IO22, not the 22nd physical pin. Connect to
DIN, not DOUT. The external supply and ESP32 must share ground. Do not power the
ring from the ESP32's 3.3V pin. With the ESP32 powered over USB, keep the external
supply's positive wire connected only to the ring, not the board's power pins.

For reliable data, use a 5V-powered 74AHCT125 or 74HCT245 level shifter between
GPIO 22 and DIN, with a shared ground and the output enabled. Put the series
resistor near DIN, after the level shifter. A direct 3.3V data signal can work
over a short wire but is not guaranteed with a 5V-powered ring.

A 500-1000 uF capacitor rated at least 10V across the ring's power input is
recommended; observe its polarity. The ring uses global brightness 255/255, but
both patterns light only a small moving section. This draws much less current than
lighting the whole ring at full white; it is not a hardware current limit.
At full-brightness white, 40 RGB pixels can require up to about 2.4A, so check
the supply and wiring capacity before switching to more demanding patterns.
The ring plus 15-pixel power cell can require up to about 3.3A for the LEDs alone at
full white. The existing USB wall adapter's rating has not been confirmed;
check its label and use appropriately rated wiring. Software brightness is not
a substitute for adequate wiring, supply capacity, and circuit protection.

### Power Cell and Optional Switches

The power cell is wired and enabled on GPIO 23. Switch assignments below are
suggestions only and remain disabled in the firmware:

| Control                 | GPIO         | Configuration setting |
| ----------------------- | ------------ | --------------------- |
| 15-pixel power cell DIN | 23 (enabled) | `POWER_BAR_PIN`       |
| Pack power toggle       | 32           | `PACK_SWITCH_PIN`     |
| Momentary firing button | 17           | `FIRE_BUTTON_PIN`     |
| Momentary vent button   | 33           | `VENT_BUTTON_PIN`     |

Set switch pins in [include/Config.h](include/Config.h) only after confirming
their wiring. Keep pins unique. The power cell is a separate LED data output,
not an extension of the ring's data chain. It uses the same external 5V power
and common-ground arrangement, with its own data resistor and level shifter
channel. `POWER_BAR_BRIGHTNESS` remains 32/255 (about 12.5%).

Switch contacts connect between the selected GPIO and ESP32 GND. Inputs use
`INPUT_PULLUP`, so closed contacts read LOW; never feed 5V into a GPIO. Keep an
illuminated switch's lamp wiring separate from its signal contacts.

For a maintained pack toggle, leave `PACK_SWITCH_MOMENTARY = false`; closed is
on and open is off. For a momentary pack button, set it to `true`; each press
toggles power. Fire and vent use normally-open momentary buttons. Switches are
debounced for 25 ms.

These standalone controls require `WAND_LINK_ENABLED = false`. With the link
enabled, the pack instead starts off and waits for C3 Activate.
When a pack switch is assigned, the firmware starts off unless a maintained
switch is already closed. With no pack switch assigned,
`AUTO_START_WITHOUT_SWITCH = true` starts directly in idle to retain the bench
chase. Set it to `false` to start off and control power through serial.

GPIO 23 drives the power cell, so the connected 3.3V SPI microSD reader instead
uses GPIO18 SCK, GPIO19 MISO, GPIO13 MOSI, and GPIO4 CS, plus 3V3 and common GND.
These SPI pins are used by SD sound playback (see SD Sound Playback below).
Do not use the default GPIO23 MOSI or feed 5V to this 3.3V reader.
The MAX98357A uses GPIO26 BCLK, GPIO27 LRCLK, and GPIO14 DIN.
Our ring occupies GPIO 22, which is the usual I2C clock pin; a future display
will also need an explicitly planned pin assignment. We are not adopting the
upstream pin map unchanged.

## Pack Audio Bring-Up

In normal `esp32dev` firmware, `AUDIO_BRINGUP_ENABLED = true` in
[include/Config.h](include/Config.h) enables a pack-only diagnostic, not a WAV
player. It starts I2S0 in standard I2S master
TX mode at 44.1 kHz, 16-bit stereo, continuously sending zero samples. Setting
`AUDIO_BOOT_BEEP = true` (default false, disabled once SD playback worked) plays
one 200 ms, 1 kHz beep (peak 8000/32767, about -12 dBFS, 10 ms faded edges) at
boot. No sound is tied to pack/wand events yet.

| MAX98357A connection | Destination |
| ------------------- | ----------- |
| VIN | External regulated 5V |
| GND | ESP32 GND and external supply ground |
| BCLK | Main ESP32 GPIO26 |
| LRC / LRCLK / WS | Main ESP32 GPIO27 |
| DIN | Main ESP32 GPIO14 |
| SD / SD_MODE | GND for hardware shutdown during bring-up |
| Speaker + and - | The two speaker terminals; neither connects to GND |

The amplifier's **SD pin means shutdown/channel select, not the SD card**.
The user confirmed that grounding this pin silences the amplifier. Its breakout
pull-up circuit has not been verified; do not connect it directly to GPIO32 or
another ESP32 pin. No firmware-controlled amplifier enable is installed.
Keep external 5V separate from the USB-powered ESP32's power pins.

The I2S task uses eight 128-frame DMA buffers and runs separately from the pack
loop so audio writes cannot block lights or UART polling. Buffers are cleared
before routing clocks/data to the amplifier, and TX underflow auto-clear is
enabled. Driver/task/write failures are reported explicitly and disable the
audio path until reset without disabling pack controls. Keep the amplifier
hardware-muted on any failure.

On the **main ESP32 USB serial port at 115200 baud**:

- `?`: print help and audio status.
- `t`: request one 440 Hz sine tone lasting one second, then return to silence.
  Both channels carry the same samples so either amplifier channel selection
  works. Peak amplitude is 1024/32767 (about -30 dBFS), with 10 ms faded edges.
  Further tone requests while busy are ignored rather than queued.
- `m`: cancel a pending tone or fade the active tone back to zero. Already
  queued DMA audio can take roughly another 23 ms to drain, plus the fade.
  This is digital silence, not a hardware shutdown control.

These commands work independently of Activate so the test can be done with the
pack lights off. Low digital amplitude is not a guarantee of safe acoustic
volume with an unknown speaker or amplifier gain; keep the speaker away from
your ears and the amplifier supply connector accessible. The current setup has
no amplifier power switch: disconnect its battery/external supply at the
connector when instructed to turn amplifier power off, without moving individual
signal wires while powered.

### First Audio Check

1. Leave amplifier SD grounded and amplifier power off during build/upload.
   Only `esp32dev` needs updating; the C3 is unchanged.
2. Confirm over pack USB that audio reports **streaming digital silence**.
   If it reports failure, keep SD grounded and amplifier power off.
3. To listen, first switch all power off before removing the temporary SD-to-GND
   connection. Leave amplifier power switched off, boot the ESP32 over USB, and
   confirm the silent-stream status again before switching amplifier power on.
4. It should remain quiet. If crackling returns, switch amplifier power off;
   do not trigger the tone or assume the noise is a WAV problem.
5. If quiet, send `t` once. Use `m` to cancel early; silence resumes automatically.
6. Power the amplifier off before resetting/unplugging the ESP32. Restore the
   hardware SD-to-GND mute with all power off when finished.

Firmware cannot guarantee quiet before initialization, during reset, or while
the ESP32 is unpowered. A verified hardware shutdown circuit is still needed
for automatic mute sequencing. Disabling `AUDIO_BRINGUP_ENABLED` stops I2S setup;
it does not physically mute the amplifier.

This deliberately differs from GPStar's UART-controlled GPStar Audio/WAV Trigger
hardware. It uses the installed ESP32 I2S driver directly, with no dependency
changes or upstream audio-source imports. Multi-track mixing and GPStar
sound-event mapping remain deferred.

### SD Sound Playback

[include/PackSdAudio.h](include/PackSdAudio.h) mounts the microSD card at boot
(SPI SCK18/MISO19/MOSI13/CS4, 20 MHz, built-in Arduino `SD` library) and indexes
files in the card root named the GPStar way: a three-digit prefix, an underscore,
and `.wav`, for example `015_afterlife_pack_startup.wav`. Tracks 001-499 are
effects and 500+ are music, matching GPStar's numbering; the track number is the
GPStar sound enum index. Other files, folders, and macOS `._` files are ignored.
The boot log reports the track count, duplicates, and gaps.

GPStar's pack does not decode audio itself: it sends play/loop/gain/fade/stop
commands over UART to a GPStar Audio or WAV Trigger board. Here the ESP32 does
that board's job with a small mixer: up to **8 tracks at once**, each with its
own loop flag, gain in dB, and fades. Fades are linear in dB, starting from or
ending at GPStar's -70 dB minimum (treated as silence). A stop is a 5 ms fade to
avoid clicks. Master volume (`+`/`-`) applies on top.

#### System themes

The pack supports GPStar's four system themes. Each sets the pack and wand
sounds, cyclotron style, and ramp timings. Choose one on the web page (Theme) or
with USB keys `1` 1984, `2` Afterlife, `3` 1989, `4` Frozen Empire. The choice is
saved in pack flash (Preferences namespace `pack`, key `theme`) and restored at
boot; `SYSTEM_THEME` in [include/Config.h](include/Config.h) is the default
until one is saved. Switching while the pack is on crossfades the hum to the new
theme's idle loop over 0.5 s, restarts the cyclotron with its spin-up, and resets
the wand sounds to the current switch positions.

| | 1984 | 1989 | Afterlife | Frozen Empire |
| --- | --- | --- | --- | --- |
| Pack startup | 379 boot + 380 hum (3.8 s fade-in) | 037 + 038 hum (3.8 s) | 015 + 016 (15 s) | 413 + 405 hum (10 s) |
| Pack shutdown | 035 | 039 | 348 | 418 |
| Fire start / loop (fade-in) | 391 / 393 (1.7 s) | 097 / 098 (0.7 s) | 155 / 098 (0.3 s) | 335 / 098 (0.3 s) |
| Tails: short / mid / long | 394 / 395 / 392 | 010 / 071 / 011 | 385 / 386 / 387 | always 386 |
| Overheat warning / vent close | 060 / 342 | 060 / 342 | 362 / 355 | 362 / 355 |
| Wand | Vent on 329, off 017; one 022 beep per arm | Same, Vent on 333 | Ramp/idle/beep loop below | Same as Afterlife |
| Cyclotron | Four windows in turn, 500 ms; previous fades over 1.5 steps | Same as 1984 | Per-LED fades below | Same as Afterlife |
| Light ramp-up / shutdown | 3 s circular / 2.5 s, then off | 3 s / 0.1 s, then off | 6 s / 10.5 s + 1.3 s fade | 6 s / 2.6 s + 1.3 s fade |

All 52 theme tracks were checked on the card (16-bit, 44.1 kHz stereo). The
GPStar quick/long startup setting (`AUDIO_LONG_STARTUP`) only changes Afterlife
and Frozen Empire; 1984 and 1989 have one startup. The volume tweaks requested
for Afterlife carry over to the matching sounds: wand boot sounds at
`WAND_READY_LEVEL_DB`, beeps at `WAND_BEEP_LEVEL_DB` and
`OVERHEAT_BEEP_LEVEL_DB`, the simplified overheat at `OVERHEAT_SOUND_LEVEL_DB`,
and the pack hum dips while the Vent is on. In 1984 and 1989, GPStar's wand
activation and overheat-recovery sounds are wand-local, so the pack plays nothing
for them. The 1984/1989 renderer (`Cyclotron1984`) lights the same four lens
windows at the same dim warm red as Afterlife; GPStar's 40-LED positions
(0, 10, 18, 28) and 3-LED lenses are adapted to our 6-pixel windows. The older
single-head `Cyclotron` class remains only for the existing native tests.

#### Afterlife pack sounds

[include/PackSounds.h](include/PackSounds.h) follows GPStar's Afterlife theme in
ProtonPack `packStartup()` and `packShutdown()`, at GPStar's default 100% effects
volume (0 dB):

| Pack event | Sounds |
| --- | --- |
| Starting (wand Activate on) | Stop 035, 348, 016. With `AUDIO_LONG_STARTUP = true` (GPStar "long startup", our default): stop and play 015 Afterlife pack startup, and play 016 idle loop looping, fading in from silence over 15 s. With it false (GPStar's default quick startup): play 001 bootup, and 016 looping with a 0.5 s fade-in. |
| Stopping (Activate off or wand link lost) | Play 348 Afterlife shutdown; stop 001, 387, 015; fade out 016 over 50 ms. |
| Firing starts (pack accepts wand fire) | Play 155 Afterlife fire start, and 098 high-power firing loop looping with a 0.3 s fade-in (GPStar `modeFireStartSounds()`, proton stream, power level 5, intensify). |
| Firing stops (release or overheat cooldown) | Play a tail chosen by firing length on GPStar's 15 s timer: 385 short (under 5 s), 386 mid (5-10 s), 387 long (over 10 s). Our 8 s limit means short or mid. Stop 155, 009, 072, 098. If the pack is shutting down instead, no tail plays. |

The idle loop keeps playing through firing and cooldown, as in GPStar. Only the
Afterlife sound theme is used, whichever cyclotron light mode is selected.

#### Wand switch sounds

Our wand has no speaker, so the pack plays the Neutrona Wand's sounds too, the
way a GPStar pack does with its default "extra pack sounds" option. The pack
reads the wand's Activate, Vent, and arm (wand-enable) switches and firing flag
from the UART link. Wand idle, ramp, and beep sounds play at
`WAND_SOUND_LEVEL_DB` (-20 dB, GPStar's `i_wand_idle_level`); the wand shutdown
sound plays at full volume. Firing sounds that both GPStar boards would play
are played once.

| Wand event (Super Hero mode) | Sounds |
| --- | --- |
| Activate on, Vent off | GPStar: stop wand sounds; play 141 ramp 1, then 142 idle 1 looping after 1.768 s. **Off by request** (`WAND_IDLE_1_SOUNDS = false`): no wand hum until the Vent is on. |
| Vent on | Play 143 ramp 2 (147, the fade-in variant, if Vent was already on when Activate was switched on); stop 141, 142, 145, 146, 148; 144 idle 2 loops after 1.881 s. |
| Vent off (wand still on) | Play 146 ramp-down 2; stop 143, 147, 144. GPStar then loops 142 idle 1 after 1.881 s (skipped while `WAND_IDLE_1_SOUNDS` is false). |
| Arm switch on (with Vent on, not firing) | 022 level-5 beep loop at `WAND_BEEP_LEVEL_DB` (-24 dB, 4 dB quieter than GPStar by request). |
| Arm switch off | Stop the beep loop at once. |
| Firing | The beep loop finishes its current pass, then restarts 1 s after firing ends. |
| Activate off | Play 017 wand shutdown; stop all wand ramp/idle sounds and the beep; play 148 ramp-down fade-out if Vent was on, otherwise 145 ramp-down 1 (skipped while `WAND_IDLE_1_SOUNDS` is false). |

Losing the wand link counts as Activate off.

By request, two levels differ from GPStar. The higher-pitched "wand ready" sounds
with the Vent on (143/147 ramp 2, 144 idle 2, and the 146/148 ramp-downs from
them) play at `WAND_READY_LEVEL_DB` (-10 dB) instead of -20 dB. While the Vent
is on, the pack startup (015) and idle (016) sounds dip by `PACK_READY_TRIM_DB`
(-6 dB). The dip is a per-track trim in the mixer that slides over about 1 s on
top of any fade in progress, so it also works during the 15 s startup fade-in.
It lifts when the Vent goes off, the wand turns off, or an overheat starts.
All overheat sounds (vent open/close, steam burst, steam loop, steam fade-out)
play at `OVERHEAT_SOUND_LEVEL_DB` (-10 dB) instead of 0 dB, also by request.

#### Overheat and vent sounds

Our 8-second firing limit is GPStar's overheat. The wand and pack cooldown was
lengthened from 3 to 10 seconds (`COOLDOWN_MS` in both
[include/WandConfig.h](include/WandConfig.h) and [include/Config.h](include/Config.h))
to fit GPStar's level 5 overheat: `i_overheating_delay` (4 s) plus
`i_ms_overheating_length_5` (6 s). Both boards must use the same value.

| Time | Sounds (GPStar function) |
| --- | --- |
| Firing, speed level 4 (a quarter of the 8 s remains) | 362 overheat warning beeps, looping (`cyclotronSpeedUp(4)`, `W_BEEP_START`), at `OVERHEAT_BEEP_LEVEL_DB` (-10 dB; GPStar uses 0 dB). |
| Overheat (8 s of firing) | Firing tail and stop sounds as above; stop 362. Pack: 341 vent open (`packOverheatingStart()`). Wand: beep loop ends naturally and the 144 idle 2 hum fades out over 0.5 s. **Simplified by request:** GPStar also plays 360 pre-vent, the 361 overheat-hot loop, 148 wand ramp-down, and 017 wand shutdown here; opening that many files at once glitched the audio. |
| +4 s | One random steam burst (061, 356-359) with a 0.12 s fade-in and 179 steam loop fading in over 1 s (`cyclotronOverheating()`). GPStar also plays 178 air release. |
| +10 s (cooldown ends) | Stop 179; play 355 vent close and 180 steam fade-out (`packOverheatingFinished()`). Wand sounds resume: ramp 2 then idle 2 with the Vent on, and the beep loop. |

If the pack shuts down during an overheat, the steam and overheat-hot loops stop
without the vent-close sounds. Pack lighting during the overheat is unchanged:
the cyclotron and power cell stay dimmed to one quarter for the whole 10 s.

#### Sound mute (wand gesture)

With the wand's **Activate switch off**, press and hold the **fire button for
3 seconds** to toggle all pack sounds off or back on. The hold must begin after
fire was released with Activate off, so letting go of Activate while still
holding fire after firing never toggles it. Release before toggling again.

- The wand saves the setting in its flash (`WandMute.h`, Preferences namespace
  `wand`, key `muted`), so it survives power-off. `MUTE_HOLD_MS` is in
  [include/WandConfig.h](include/WandConfig.h).
- The wand sends it in every link status packet as flag bit 7 (`wand_link::Muted`,
  formerly reserved and rejected). Both boards must run firmware that knows it.
- The pack fades all SD sounds out or in over about 60 ms. They keep running
  silently, so unmuting brings back whatever loop is playing. Lights are unaffected.
- Confirmation: the pack plays a short beep that the mute does not silence: a low
  500 Hz beep when muting, a high 1 kHz beep when unmuting. No beep plays when the
  pack learns the saved state at connect. The `t` tone also ignores the mute.
- The pack keeps the last mute state if the wand link drops, so the shutdown
  sounds that follow stay muted.

GPStar's equivalent master mute lives in its wand settings menu (barrel wing
button), which this wand does not have.

#### USB commands

- `#NNN` then Enter: play a track once, e.g. `#15` or `#015`, alongside anything
  already playing. Type with the monitor's `--echo` option to see the digits.
- `d`: mount the card if needed, rescan, and list every indexed track.
- `+` / `-`: master volume up/down 3 dB, from -40 to 0 dB. It starts at
  `SD_AUDIO_DEFAULT_DB` (0 dB, full scale). Track 015 played at 0 dB without
  ESP32 resets on USB 5V amp power; lower it if loud peaks cause resets.
  For more volume than 0 dB, change the amp's hardware gain (GAIN pin), not software.
- `m`: stop all tracks (including loops) and the test tone.
- `?`: also prints SD status and active voices.

Supported files: uncompressed 16-bit PCM at 44.1 kHz, mono or stereo (GPStar's
required format). Metadata chunks are skipped. Stereo is mixed down to mono
because the MAX98357A drives one speaker. A reader task streams each track into
its own 8 KB (~93 ms) buffer; a track starts once ~46 ms is buffered, so sounds
start about that much after the event. If all 8 voices are busy, a new track
waits up to 20 ms for a stopping voice to free up, refilling playing voices
meanwhile. Opening a file scans the card's root directory (893 entries here,
about half of them macOS `._` files), so the reader opens at most one new file
per pass and refills every playing voice in between; starting many sounds at
once can still glitch, which is why the overheat sequence is trimmed. Running
`dot_clean -m /Volumes/<card>` on the Mac before ejecting removes the `._`
files and shortens those scans.

Known and accepted (2026-10-07): with Wi-Fi on and the `._` files removed, a
session logs a few dozen underruns, concentrated at Vent on (143/147, then 144
opening) and at pack shutdown (017, 148, and 348 opening together). Each is one
voice short for part of a 2.9 ms block and was not audible. Lowering volume does
not help (it is a read-speed issue, applied after reading). If ticks become
audible: open fewer files at those moments, move the SD reader to core 1 away
from the Wi-Fi radio, or try a faster SD clock. Loops restart from the start of the audio data.
The log reports buffer underruns if the card cannot keep up with several tracks.
Tracks mix with the optional boot beep and the `t` tone. The pack runs normally
without a card.

The SD reader task runs on core 0 at priority 2 and reads files with POSIX
`read()` in up to 8 KB chunks. Buffered stdio reads, sharing core 1 with the
never-idle Arduino loop, could not keep up with 3+ simultaneous stereo tracks
(hundreds of underruns per second, heard as static). The I2S output task runs at
priority 3 on core 0, so SD reads never delay it.

A peak limiter in [include/PackAudio.h](include/PackAudio.h) keeps the mix from
clipping when loud tracks overlap (for example fire start, firing loop, and the
idle hums at 0 dB). Each 2.9 ms block is checked before it plays; if it would
exceed 32000/32767, its gain drops for that block, then recovers over about
0.3 s. Quiet passages stay at full volume. The log reports any clipped samples
the limiter misses and any buffer underruns, once per second. It also reports how
many blocks the limiter reduced and the deepest reduction; the web status shows
the same count as "Limited".

The 1984 and Frozen Empire hums (379/380, 413/405) have a faint fizz on the pack
speaker that is in the recordings. Most of their energy is a 69 Hz hum, the 1984
files clip at full scale, and the 200 Hz-4 kHz range holds only low-level grit.
A small speaker reproduces little 69 Hz, so the grit stands out; full-range
speakers mask it. Supply, limiter, mono downmix, filters, rattles, and loud 69 Hz
and 220 Hz test tones were all ruled out (2026-10-08). Accepted; a larger speaker
would help.

### Basic Direct-I2S Diagnostic

The temporary `pack_audio_test` target builds
[src/pack_audio_test.cpp](src/pack_audio_test.cpp), not the normal pack entry point.
It clears the 40-pixel cyclotron and lights all 15 power-cell pixels steady blue
once before starting audio, using the normal `POWER_BAR_BRIGHTNESS` of 32/255.
There are no ongoing LED updates. The steady power cell adds a load intended to
keep the shared battery supply awake; the battery's minimum-load threshold is
unknown, so this still needs a hardware check. It does not initialize the wand
UART, poll pack controls, or run pack animations. The C3 firmware is unchanged,
but receives no pack acknowledgements while this target is installed.

If the LED supply was off at startup or has gone to sleep, reconnect/wake it and
send USB `l` (lowercase L) to resend the static LED state once before a tone test.
NeoPixels that were unpowered can miss the startup command. This manual refresh
does not start an animation or periodic LED transmission. Stop the tone and
allow queued audio to drain before refreshing lights; `l` is rejected while a
tone is active.

This target now uses the user's minimal sine-wave test directly in `loop()`,
not [include/PackAudio.h](include/PackAudio.h) or a separate audio task. It keeps
I2S0, BCLK26/LRCLK27/DIN14, standard I2S, 44.1 kHz, and 16-bit stereo, but uses
eight 256-frame DMA buffers. Each block contains identical left/right samples
from a wrapped phase accumulator. Silence is continuously written until a
tone is requested.

On the pack USB port at 115200 baud:

- `t`: start a **continuous 1 kHz tone**, peak **8000/32767 (about -12 dBFS)**.
  It does **not** stop after one second. Repeated `t` does not restart it.
- `m`: return to digital silence. Allow roughly 46 ms for queued DMA audio
  to drain. This is not hardware mute.
- `l`: resend the static LED state once while the tone is stopped.
- `?`: print the basic diagnostic banner, audio state, and settings.

This tone has about **18 dB higher digital amplitude** than the previous
440 Hz diagnostic. A quarter-scale sample value is not a guarantee of low
speaker volume. Keep the speaker away from your ears and the amplifier supply
connector accessible. There is no attack/release envelope in this basic test,
so switching the tone can produce a click; distinguish an edge click from
crackling throughout playback. **Do not use the old one-second tone script
without adding an explicit `m` stop.**

Unlike the supplied automatic-start example, this version boots into silence,
as selected by the user. It explicitly leaves MCLK unassigned, clears DMA before
routing the I2S pins, checks driver calls/write lengths, and uses a 100 ms write
timeout rather than blocking forever. A driver failure is logged and disables
audio until reset; static lighting and USB commands remain available.

This removes the separate audio task as well as ongoing pack LED transmission
and wand-link processing. It also changes tone frequency, amplitude, duration,
envelope, and block size, so it is not a single-variable comparison. It does not
isolate the speaker, supply, wiring, or the I2S driver itself. Normal
[include/PackAudio.h](include/PackAudio.h) remains unchanged with its original
one-second 440 Hz, -30 dBFS tone. GPStar's UART-controlled external audio backend
is still not used by this direct-I2S diagnostic.

Disconnect the amplifier supply for uploading. Select the main ESP32's current
port explicitly, never the C3 port:

```sh
~/.platformio/penv/bin/pio run -e pack_audio_test
~/.platformio/penv/bin/pio run -e pack_audio_test -t upload --upload-port <pack-port>
```

Uploading this target **replaces normal pack firmware**. Confirm the
`Pack BASIC I2S diagnostic` banner and silent-stream status over USB before
connecting/waking the amplifier supply. Send `l` if the power cell missed its
startup command. For a brief listening check, send `t`, then explicitly send
`m` to stop; disconnect amplifier power if unexpectedly loud noise occurs.
No SD playback or unit tests are added.

Restore normal pack lighting and wand control by uploading `esp32dev` again,
with the amplifier supply disconnected:

```sh
~/.platformio/penv/bin/pio run -e esp32dev -t upload --upload-port <pack-port>
```

## Pack Wi-Fi Control Page

The pack can run its own Wi-Fi network with a phone-friendly control page,
following GPStar's pack web UI (ESPAsyncWebServer, LGPL-3.0; no GPStar source
copied). Code: [include/PackWeb.h](include/PackWeb.h) (access point, routes,
queues) and [include/PackWebPage.h](include/PackWebPage.h) (the page, embedded
in the firmware).

### Setup

1. Copy `include/WifiSecrets.example.h` to `include/WifiSecrets.h` and set
   `AP_PASSWORD` (8-63 characters). Optionally set `AP_SSID`; empty means
   `ProtonPack_XXXX` from the last MAC bytes. `WifiSecrets.h` is git-ignored.
   Without it the pack builds with Wi-Fi off and says so at boot.
2. Build and upload `esp32dev`. The boot log prints the network name and URL.
3. On your phone, join that network (it has no internet), then open
   **http://192.168.4.1/**.

`WIFI_ENABLED`, `WIFI_CHANNEL` (1, like GPStar), and `WIFI_MAX_CLIENTS` (4)
are in [include/Config.h](include/Config.h).

### What the page does

- **Status**, refreshed twice a second: pack state and firing speed level; wand
  link, Activate, Vent, Arm, firing, and overheat; SD card and track count;
  Wi-Fi clients; buffer underruns, clipped samples, and limited blocks; uptime.
- **Mute**: the latest change wins, from the page or the wand gesture. The wand's
  saved setting applies at connect; after that each wand gesture toggles the
  current state. Changes beep like the wand gesture.
- **Volume**: master volume, -40 to 0 dB. Not saved across power-off yet.
- **Pack name**: one name for the Wi-Fi network and the Bluetooth speaker, saved
  on each pack (Preferences `pack/name`, 1-24 letters, digits, spaces, `- _ .`).
  Blank restores the default `ProtonPack_XXXX`. Saving restarts the pack; join
  the renamed network afterwards (same password). Not yet tested on hardware.
- **Music**: lists tracks numbered 500+ on the card; play one (replacing any
  music already playing) or stop it. Music plays at `MUSIC_LEVEL_DB` (-6 dB) for
  mix headroom. While music plays, all pack and wand effects (tracks below 500)
  duck by `EFFECTS_UNDER_MUSIC_DB` (-10 dB), sliding over about 1 s, and return
  when the music stops or ends (a group trim in the mixer). Music is not ducked. Add songs as 16-bit 44.1 kHz stereo WAVs
  named `501_name.wav`, `502_...` with no gaps, then restart the pack.

### Firmware updates over Wi-Fi

After one USB upload of `esp32dev`, the pack accepts PlatformIO uploads over its
own network (ArduinoOTA on port 3232, `OTA_ENABLED` in
[include/Config.h](include/Config.h)). Join the pack's Wi-Fi, then run:

```sh
~/.platformio/penv/bin/pio run -e pack_ota -t upload
```

`pack_ota` builds the same firmware as `esp32dev` and sends it to 192.168.4.1.
The password is the pack Wi-Fi password: [scripts/ota_auth.py](scripts/ota_auth.py)
reads `AP_PASSWORD` from the git-ignored `include/WifiSecrets.h`, so keep it a
plain quoted string without backslashes. When the upload starts, the pack stops
its sounds and turns its lights off. The loop pauses, so the wand sees link loss.
The new image goes to the spare app partition and the pack reboots into it only
after it is verified. If anything fails, the old firmware keeps running. Allow
incoming connections if the macOS firewall asks, because the pack connects back
to the Mac. The wand still uploads over USB. OTA costs about 2 KB of heap (about
35 KB free after Wi-Fi and OTA).

### API

| Request | Effect |
| --- | --- |
| `GET /` | Control page |
| `GET /api/status` | Status JSON |
| `GET /api/music` | Music tracks `[{"n":500,"name":"500_x.wav"}]` |
| `POST /api/mute?on=1` or `on=0` | Mute or unmute |
| `POST /api/volume?db=-12` | Master volume (-40 to 0 dB) |
| `POST /api/music/play?track=500` | Play a music track |
| `POST /api/music/stop` | Stop music |
| `POST /api/bluetooth` | Restart as a Bluetooth speaker |

Memory is the tight resource. With 4 KB FAT sectors this core reserves about
4.6 KB per allowed open file in one contiguous block, so the SD card mounts
first (8 open files, one per voice), before the voice buffers and Wi-Fi. Boot
logs free heap after each stage; with Wi-Fi up about 43 KB remains (16 KB voice
buffers left 7 KB and broke the web server). The AsyncTCP stack is 8 KB
(`CONFIG_ASYNC_TCP_STACK_SIZE`), and track names are kept in one packed buffer.

Web handlers run in the AsyncTCP task, pinned to core 1 (`CONFIG_ASYNC_TCP_RUNNING_CORE=1`)
away from the core 0 I2S and SD tasks. They only queue commands and copy JSON
snapshots that the loop task publishes, so they never touch pack objects. The
Wi-Fi radio itself runs on core 0; watch the page's underrun count. Wi-Fi adds
transmit current bursts (around 300 mA), so a solid 5 V supply matters.

Later steps: saving volume and settings across power-off, changing the Wi-Fi
password from the page, and firmware updates over Wi-Fi.

## Pack Bluetooth Speaker Mode

The pack can restart as a Bluetooth speaker (A2DP sink) named
`ProtonPack_XXXX` (its Bluetooth MAC is one above the Wi-Fi MAC, so the suffix
differs from the Wi-Fi network). Pair from the phone's Bluetooth settings; no PIN
is needed. Phone audio plays through the pack amplifier at `MUSIC_LEVEL_DB`; the
phone's volume sets the level. One phone connects at a time; the pack stops
being discoverable while connected.

- **Enter:** press "Switch to Bluetooth" on the web page, send USB `b`, or use
  the wand gesture.
- **Leave:** power-cycle the pack, send USB `b`, or use the wand gesture.
- **Wand gesture:** with Activate (left toggle) off, flip Vent (top right toggle)
  on and off three times within 3 seconds (`MODE_GESTURE_MS`). The pack beeps
  and restarts in the other mode; the log shows each counted flip.

In Bluetooth mode, Wi-Fi, the web page, OTA, and **pack sounds are off**: the
Bluetooth stack needs the RAM the SD mixer uses (pairing failed with about 2 KB
left, and iOS reported "not supported" or "pairing took too long"). Lights, the
wand link, mute (it also silences phone audio), and confirmation beeps still
work. Heap stays about 58 KB or more while streaming.

Implementation: [include/PackBluetooth.h](include/PackBluetooth.h) uses the
ESP-IDF Bluedroid API built into the Arduino core (A2DP sink with AVRCP
controller and target, "just works" pairing, loudspeaker device class). It mixes
SBC-decoded stereo as mono into the pack output through a 16 KB buffer with
one-sample drift correction, and resamples 48 kHz streams to 44.1 kHz. The mode
survives only a software restart (`RTC_NOINIT_ATTR` flag plus `ESP_RST_SW`), so
any power-on, upload, or crash boots back to Wi-Fi. Wi-Fi mode returns the
Bluetooth controller and Bluedroid RAM to the heap at boot (`esp_bt_mem_release`),
leaving about 32 KB free after Wi-Fi and OTA. The pack uses `min_spiffs.csv`
(two 1.9 MB app slots, so OTA still works); firmware is about 1.71 MB. Changing
the partition table needs one USB upload; saved settings are kept. Not a GPStar
feature.

## Simple Pack (Second Build)

A simplified pack on a third ESP32-WROOM, with no wand board. It is a separate
build (`simple_pack`, [src/simple_pack.cpp](src/simple_pack.cpp) and
[include/simple/](include/simple/)); the main pack and wand code is included
unchanged, so the simple pack has the same sounds, themes, timing, firing limit,
overheat and mute gesture.

| Item | GPIO |
| --- | --- |
| Cyclotron: 4 x 7-pixel jewels (28) | 22 |
| Power cell: 12-pixel strip | 23 |
| Wand chain (10): 4-pixel bargraph, blue body light, white body light, vent, 3-pixel firing strip | 21 |
| Activate switch (to GND) | 33 |
| Fire button (to GND) | 32 |
| Amp BCLK / LRCLK / DIN | 26 / 27 / 14 |
| SD SCK / MISO / MOSI / CS | 18 / 19 / 13 / 4 |

Activate starts the pack and wand (Vent and arm follow it); fire works after the
1.5-second startup with a fresh press. Each jewel shows one of the main pack's
6-pixel lens windows on its outer ring, with the centre following the brightest
pixel; `CYCLOTRON_GAIN_PERCENT` brightens the main pack's dim warm red. The wand
bargraph follows the theme like the main wand's. The 5th wand pixel (behind a
blue lens) is blue. By request the armed-wand beep loop (`ARM_BEEPS`) and the
Afterlife/Frozen Empire wand-ready whine (`WAND_READY_HUM`, tracks 147/144) are
off. Settings are in
[include/simple/SimpleConfig.h](include/simple/SimpleConfig.h).

Wi-Fi: the simple pack runs its own network (`ProtonPack_XXXX`, same password as
the main pack, from `include/WifiSecrets.h`) with a control page at
**http://192.168.5.1/**: theme (saved), mute, volume and music. It uses
192.168.5.1 instead of the main pack's 192.168.4.1, so a Wi-Fi upload can't reach
the wrong pack. USB keys 1-4 also change the theme. No Bluetooth mode.

Power: holding fire caused brownout resets when the ESP32 shared the amp's 5V
battery feed (the page's System line shows the last restart reason). Give the
ESP32 its own 5V supply or regulator, and run the amp and pixels on separate thick
wires from the battery with all grounds joined. The simple pack also defaults to
-6 dB master volume (`DEFAULT_VOLUME_DB`; the web/USB volume is saved), dims the
wand bargraph to 25 % (`BARGRAPH_LEVEL_PERCENT`), and after a brownout releases a
stuck SD card and retries the mount so sound returns.

Firmware over Wi-Fi: join the simple pack's network, then
`~/.platformio/penv/bin/pio run -e simple_pack_ota -t upload` (the boot log's OTA
line names the main pack's `pack_ota` env; use `simple_pack_ota` here).

```sh
~/.platformio/penv/bin/pio run -e simple_pack -t upload --upload-port <port>
```

[scripts/require_mac.py](scripts/require_mac.py) refuses the upload unless the
board's MAC is the simple-pack board's (04:b2:47:92:12:8c), so the main pack
cannot be overwritten by mistake. The Freenove breakout's 5V pins are fed by its
barrel jack, not USB.

## Legacy Pack Build and Upload

Open this folder in VS Code with the PlatformIO IDE extension. In the
PlatformIO sidebar, use **Project Tasks > esp32dev > General > Build**, then
**Upload**. Uploading replaces the firmware currently on the ESP32. Build alone
does not change the device. Select `esp32dev` explicitly for the pack; the
project default is now the C3 wand. Do not upload this target to the C3.

Alternatively, using the existing PlatformIO installation on macOS:

```sh
~/.platformio/penv/bin/pio run --environment esp32dev
~/.platformio/penv/bin/pio run --environment esp32dev --target upload
~/.platformio/penv/bin/pio device monitor --environment esp32dev --baud 115200
```

PlatformIO detects the serial port automatically. If several devices are
connected, use `pio device list` and add `--upload-port <port>` to the upload
command, or `--port <port>` to the monitor command. Close the serial monitor
before uploading. Exit the monitor with Ctrl+C.

## Pack Effects

Afterlife is the default cyclotron mode. A rotating sequence triggers independent
LED fade envelopes across four window groups, skipping the gaps between them.
Each LED fades in with a circular-in curve, then out with a circular-out curve,
adapting GPStar's Afterlife rendering instead of the earlier fixed four-pixel
GBPack32-inspired head. At steady idle, fade-in and fade-out each take three
motion steps; during speed ramps or accelerated firing the fade-in takes one
step. Each triggered LED retains its own timing until its fade completes.

Startup and shutdown follow GPStar's Afterlife light timing, which lines up with
the startup (015) and shutdown (348) sounds. Startup ramps from 857 ms to 20 ms
per step over 6 seconds (quartic-out). That is GPStar's `i_2021_ramp_delay`
(300 ms) and `i_2021_ramp_length`, scaled by the same x20/7 as our idle speed
(GPStar's 40-LED idle is 7 ms). Shutdown keeps the cyclotron at full brightness
while it slows back to 857 ms over 10.5 seconds (quartic-in,
`i_2021_ramp_down_length`). It then fades out over 1.3 seconds, like GPStar's
`fadeOutCyclotron()`; the total of 11.8 s matches track 348. While the
cyclotron ramps, each power-cell step also waits the current cyclotron step
(GPStar `powercellLoop()`), so the power cell keeps cycling, speeding up and
slowing down with it, instead of filling or draining.
With 24 visible pixels, a steady idle lap takes about 0.48 seconds. The existing
warm-red tint and dim peak range (20-51/255 before global brightness scaling)
are retained rather than adopting GPStar's full configured per-LED peak.

The window layout adapts GBPack32's four groups of four LEDs on a 40-pixel ring
to four groups of six LEDs on this 40-pixel ring, retaining the existing window
width after the ring count change. With zero-based numbering, the default groups
are 37, 38, 39, 0, 1, 2; 7-12; 17-22; and 27-32. This assumes
evenly spaced windows centered around pixels 0, 10, 20, and 30. Adjust
`AFTERLIFE_WINDOW_OFFSET` to rotate that layout and `AFTERLIFE_WINDOW_PIXELS`
to match the actual opening width. Pixels between the windows stay dark.

The 1984 rendering is retained as a separate pattern. It moves a bright
red head through every ring pixel at 95 ms per step, halving the trail on each
step (255, 127, 63, 31, 15, 7, 3, 1, then off). A 40-pixel idle lap takes about
3.8 seconds. This is GBPack32's default 1984-style idle chase, not the earlier
interpolated three-second test.

Set `CYCLOTRON_MODE` in [include/Config.h](include/Config.h) to
`CyclotronMode::Afterlife` or `CyclotronMode::Classic1984` to choose the startup
mode. Serial `1` and `2` switch modes without uploading. A restart returns to
the configured default; runtime selection is not saved to flash.

The blue power cell continues filling all 15 pixels, clearing, and repeating,
regardless of the selected cyclotron mode. Its position accumulates over time,
so changing speed does not jump to a different point in the fill. LED frames
update every 5 ms.

### Wand-Driven Overheat Acceleration

Like GPStar, the wand advances the pack speed level when remaining firing time
falls below one-half, one-third, and one-quarter of the overheat limit, capped
at level 4. With the current eight-second limit:

| Firing elapsed | Speed level | Afterlife step | Power-cell step |
| -------------- | ----------- | -------------- | --------------- |
| 0-4000 ms | 1 | 20 ms | 75 ms |
| 4001-5334 ms | 2 | 14 ms | 70 ms |
| 5335-6000 ms | 3 | 11 ms | 65 ms |
| 6001-7999 ms | 4 | 9 ms | 60 ms |
| 8000 ms | Cooldown | 20 ms | Dimmed full bar |

The first half of firing deliberately stays at normal speed. Each later speed
change applies immediately, without the old three-second firing ramp masking
the short stages. Release, cooling, and disconnect clear the firing speed level;
shutdown still slows down through its separate ramp. The existing safety timers
and fresh-press guards are unchanged.

GPStar's 40-LED Afterlife step intervals are 7, 5, 4, and 3 ms. These ratios are
scaled and rounded to our 20 ms window-chase base, not copied as literal full-ring
timings. Power-cell intervals follow GPStar's modern-mode reductions of 5, 10,
and 15 ms. Linked 1984 mode instead divides its 95 ms idle interval by levels
1-4, matching GPStar's classic speed-multiplier approach.

The level is sent as an absolute value in every heartbeat, so a missed stage
packet cannot permanently lose an increment. The pack only applies it while
actually firing and reports level changes over USB. With `WAND_LINK_ENABLED =
false`, standalone firing retains the previous 15 ms Afterlife target (ramped)
or 30 ms 1984 target, and the power cell's half-delay firing preset.

The pack state controls the animations:

| State    | Behavior                                                                                  |
| -------- | ----------------------------------------------------------------------------------------- |
| Off      | All configured LEDs off                                                                   |
| Starting | 1.5-second fade in, then idle; Afterlife speed ramp-up continues for 6 s                  |
| Idle     | Selected cyclotron effect; blue bar fills repeatedly                                      |
| Firing   | Wand-driven staged cyclotron and power-cell speeds as overheat approaches |
| Cooling  | LEDs dim to one-quarter of their configured brightness for 10 seconds (overheat)          |
| Stopping | Afterlife: 10.5 s slowdown at full brightness, 1.3 s fade, then off (11.8 s)              |

This is an independent behavioral adaptation of GPStar's Afterlife fading,
speed ramps, and staged acceleration, retaining the confirmed local pin
assignments, window layout, and 1984 renderer. No upstream application source
was imported. Startup and shutdown timings now follow GPStar's Afterlife ramps
and line up with the SD startup and shutdown sounds; lights still run on their
own timers rather than waiting for audio.
Switching cyclotron modes resets the selected animation but does not change
pack state, restart its timers, or reset the power cell.

The preserved native pack tests still include expectations for the old fixed
Afterlife head, linear ramp, and earlier ring layout. Those effect-specific
expectations need a separately authorized update; they have not been removed,
weakened, or run for this bring-up change.

Releasing fire returns to idle. Holding fire for 8 seconds triggers cooldown.
After cooldown, release and press fire again to restart; holding a button
through startup or cooldown does not automatically start firing. Pack off
interrupts startup, firing, and cooldown. Vent requests cooldown only from
idle or firing and never activates physical smoke hardware.

Startup and shutdown complete using timers, not audio playback callbacks.
Pack startup and shutdown play GPStar's Afterlife sounds (see SD Sound
Playback); lights never depend on the SD card or wait for audio.
Smoke hardware will need its own verified safe-off behavior, voltage
and driver selection, and activation limits before connecting it.

### Serial Test Controls

At 115200 baud, the monitor prints the LED configuration, selected mode, and
each state or mode change. Mode selection is always available. The power,
fire, and vent commands apply only when their hardware input pin is still
`-1`; they do not override assigned physical inputs.

| Character | Action                       |
| --------- | ---------------------------- |
| `1`       | Select 1984 cyclotron        |
| `2`       | Select Afterlife cyclotron   |
| `p`       | Toggle pack power            |
| `f`       | Press and hold fire          |
| `s`       | Release fire                 |
| `v`       | Request lights-only cooldown |
| `?`       | Print command help           |

Line endings are ignored. After automatic cooldown, send `s` and then `f` to
fire again. Exit the monitor with Ctrl+C, not `s`.

## Existing Pack Tests and Version Control

Run the state tests on the Mac, without uploading or requiring any hardware:

```sh
~/.platformio/penv/bin/pio test --environment native
```

Tests cover boot modes, startup without audio, firing/release, shutdown,
cooldown, held-button behavior, interrupted transitions, and `millis()` rollover.
Animation tests cover both cyclotron patterns, Afterlife window mapping, head
shading, speed ramping, wraparound, and mode changes preserving pack-state timing.
Native tests do not validate real LED data, wiring, or physical switch contacts.

Build products under `.pio/` are ignored; keep credentials and audio assets
out of version control.

## Legacy Pack Troubleshooting

- **Upload stuck at Connecting:** hold the board's BOOT button until uploading
  starts, then release it. Press EN / RESET after upload if necessary.
- **No LEDs:** check the external supply voltage, common ground, GPIO 22 for
  the ring or GPIO 23 for the power cell, and that data reaches DIN rather than
  DOUT. Check that the pack is not off. Serial state messages
  confirm firmware activity but cannot confirm that the LEDs receive data.
- **Flickering or random colors:** check power connections and use a level
  shifter and short data wire. Do not increase brightness to fix a data issue.
- **Red and green swapped:** check the ring's specified color order and change
  `NEO_GRB` to `NEO_RGB` if appropriate.
- **RGBW ring:** change `NEO_GRB` to the ring's specified four-channel order,
  usually `NEO_GRBW`. These patterns do not test a dedicated white channel.
- **An optional control does nothing:** its pin must be assigned before using
  physical hardware. Serial commands apply only while that input is unassigned.
- **Afterlife lights miss the lenses:** adjust `AFTERLIFE_WINDOW_OFFSET` and
  `AFTERLIFE_WINDOW_PIXELS` to match the ring's mounted orientation and windows.

## License

Copyright (C) 2026 Peter

This project is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. It is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See [LICENSE](LICENSE) for the full text.

GPL-3.0-or-later matches the projects this firmware builds on: its behaviour is
adapted from [GPStar Core Source](https://github.com/GPStarTechnologies/GPStar-Core-Source)
(GPL-3.0-or-later), and the wand links the GPLv3
[simple ht16k33 library](https://github.com/lpaseen/ht16k33). Other dependencies
(Adafruit NeoPixel, Bounce2, ESPAsyncWebServer, the Arduino-ESP32 core) carry
their own GPL-compatible licenses. No sound files are included; supply your own.
