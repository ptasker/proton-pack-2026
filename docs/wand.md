# Wand

The wand firmware (`wand_c3`, [src/wand.cpp](../src/wand.cpp)) reads the
switches and fire button, drives its own lights and bargraph, and reports its
state to the pack. The wand has no speaker; the pack plays its sounds.

Settings are in [include/WandConfig.h](../include/WandConfig.h). Wiring is in
[hardware.md](hardware.md).

## Switch sequence

The staged start-up follows GPStar's Super Hero mode.

1. **Activate on** starts a 1.5-second fade-in. The slo-blo and both hat lights
   come on. Activate off clears every light and interrupts any state.
2. **Vent on** lights the orange vent jewel. It is one of two firing gates.
3. **Wand switch on** lights the Clippard. It is the other firing gate.
4. With both gates on, a fresh press of the fire button fires. Releasing stops.

Turning either gate off stops firing. Turning it back on while fire is still
held does not restart firing: release and press again.

On this build the Vent switch is disabled and follows the wand switch, so the
lower right toggle does steps 2 and 3 together (see [hardware.md](hardware.md)).

## Firing and overheat

- Firing lights only the 23-pixel strip: red and blue bursts of 2-6 pixels
  moving along it, with a white flash at the tip.
- Holding fire for 8 seconds (`MAX_FIRE_MS`) triggers a 10-second cooldown
  (`COOLDOWN_MS`). The indicators blink during cooldown, and presses made
  during it are not queued.
- As the limit approaches, the wand raises a speed level from 1 to 4 (when less
  than a half, a third and a quarter of the time remains). The pack uses it to
  speed up its lights.

`COOLDOWN_MS` and `MAX_FIRE_MS` must match the pack's values in `Config.h`.

## Bargraph

The 28-segment bargraph follows the pack's theme, with timings adapted from
GPStar.

| State | 1984 / 1989 | Afterlife / Frozen Empire |
| --- | --- | --- |
| Activation | Sweeps up and back at 24 ms per segment | Fills from the bottom at 64 ms per segment, then holds |
| Idle | Repeating up-and-down sweep, 12 ms per segment, 90 ms pause at each end | All segments lit |
| Firing | Paired lines from the centre out and back, 15 ms per step, faster near overheat (13, 11, 9, 7 ms) | Same |
| Cooldown, Activate off | Drains from the top at 80 ms per segment | Same |

The pack sends its theme to the wand on connect, on change and every 500 ms.
The wand saves the last theme it received, so the bargraph is right even before
the pack connects. There is no rotary encoder, so the bargraph always shows full
power.

If the bargraph is missing or fails at boot, the wand carries on without it and
says so in the boot log.

## Sound mute

With Activate off, press and hold fire for 3 seconds (`MUTE_HOLD_MS`) to toggle
all pack sounds. The hold must start after fire was released with Activate off.
The wand saves the setting and sends it to the pack in every status packet. The
pack confirms with a low beep for mute and a high beep for unmute.

## Serial commands

At 115200 baud the wand reports its configuration and each state change.

| Key | Action |
| --- | --- |
| `?` | Print inputs and help |
| `p` `v` `w` | Toggle Activate, Vent, wand switch |
| `f` / `s` | Press / release fire |

The control keys only work for inputs whose pin is set to `-1`, so they cannot
override an installed switch.

## Bench and diagnostic modes

Set these flags in `WandConfig.h` and upload again. All are `false` by default.

| Flag | Effect |
| --- | --- |
| `LED_DIAGNOSTIC_MODE` | Ignores all inputs. A dim white pixel walks the chain, then the whole chain shows red, green, blue and white |
| `BENCH_IDLE_FIRING_MODE` | Treats all three switches as on, so the fire button alone works. Starts in idle with no fade-in |
| `BARGRAPH_DIAGNOSTIC_MODE` | With Activate on, lights one bargraph segment at a time, 250 ms each |
| `BENCH_ENABLE_WITHOUT_VENT_SWITCH`, `BENCH_ENABLE_WITHOUT_WAND_SWITCH` | Treats a missing (`-1`) switch as on |

The `bargraph_scan_c3` build is a separate I2C scanner for the bargraph driver;
see [development.md](development.md).

## Code map

| File | Role |
| --- | --- |
| [WandConfig.h](../include/WandConfig.h) | Pins, pixel sections, colours, timings |
| [WandController.h](../include/WandController.h) | States and firing gates |
| [WandInputs.h](../include/WandInputs.h) | Debounced switches and serial inputs |
| [WandLights.h](../include/WandLights.h) | NeoPixel output by section |
| [WandBargraph.h](../include/WandBargraph.h) | Bargraph animations |
| [WandMute.h](../include/WandMute.h) | Mute gesture and saved setting |
| [WandPackSender.h](../include/WandPackSender.h) | Status packets to the pack |
| [WandPackLink.h](../include/WandPackLink.h) | Framing and CRC shared with the pack |
