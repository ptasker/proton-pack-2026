# Pack lights and states

The pack firmware (`esp32dev`, [src/main.cpp](../src/main.cpp)) follows the
wand over the UART link. It boots off and waits for the wand's Activate switch.
Settings are in [include/Config.h](../include/Config.h).

## States

| State | Lights |
| --- | --- |
| Off | All off |
| Starting | 1.5-second fade-in; the cyclotron spins up over its theme's ramp |
| Idle | Cyclotron at idle speed; power cell fills, clears and repeats |
| Firing | Cyclotron and power cell speed up in stages as overheat approaches |
| Cooling | Everything dims to a quarter for 10 seconds |
| Stopping | Cyclotron slows down, then goes dark (timing depends on theme) |

Start-up and shutdown run on timers that line up with the sounds, but the
lights never wait for audio or need the SD card.

The pack keeps its own 8-second firing limit as well as following the wand's.
If the wand link is lost for 500 ms, the pack stops firing and shuts down.

## Cyclotron

The theme chooses the cyclotron style.

**Afterlife and Frozen Empire.** A rotating sequence triggers independent fades
on each LED across the four lens windows, skipping the gaps between them. At
idle a step takes 20 ms, so a lap of the 24 visible pixels takes about half a
second. Start-up ramps from 857 ms to 20 ms per step over 6 seconds. Afterlife
shutdown slows back down over 10.5 seconds and then fades out over 1.3 seconds;
Frozen Empire uses a quarter of that slowdown.

**1984 and 1989.** The four windows light in turn every 500 ms, each fading out
over 1.5 steps. Start-up ramps over 3 seconds. Shutdown takes 2.5 seconds in
1984 and 0.1 seconds in 1989, with no fade-out.

Both styles use a dim warm red (peak 20-51 out of 255) rather than GPStar's
full brightness.

## Speed-up while firing

The wand sends a speed level with every status packet. With the 8-second limit:

| Firing time | Level | Afterlife step | Power-cell step |
| --- | --- | --- | --- |
| 0-4 s | 1 | 20 ms | 75 ms |
| 4-5.3 s | 2 | 14 ms | 70 ms |
| 5.3-6 s | 3 | 11 ms | 65 ms |
| 6-8 s | 4 | 9 ms | 60 ms |

In 1984 and 1989 the step interval is divided by the level instead. Because the
level is sent as an absolute value, a lost packet cannot leave the pack at the
wrong speed.

## Serial commands

At 115200 baud on the pack's USB port:

| Key | Action |
| --- | --- |
| `1` `2` `3` `4` | Theme: 1984, Afterlife, 1989, Frozen Empire (saved) |
| `t` | One-second 440 Hz test tone |
| `m` | Stop all audio |
| `#NNN` Enter | Play a track, for example `#15` |
| `d` | Rescan the SD card and list tracks |
| `+` / `-` | Master volume up or down 3 dB |
| `b` | Switch between Wi-Fi and Bluetooth speaker mode |
| `?` | Help and audio status |
| `p` `f` `s` `v` | Power, fire press, fire release, vent. Standalone mode only |

## Standalone mode

Set `WAND_LINK_ENABLED = false` to run the pack without a wand. It then uses
its own switch pins (see [hardware.md](hardware.md)) or the `p`, `f`, `s`, `v`
serial keys, and with no power switch assigned it starts straight into idle
(`AUTO_START_WITHOUT_SWITCH`).

## Code map

| File | Role |
| --- | --- |
| [Config.h](../include/Config.h) | Pins, pixel counts, timings, audio levels |
| [PackController.h](../include/PackController.h) | Hardware-independent state machine |
| [PackWandReceiver.h](../include/PackWandReceiver.h) | Applies wand state and handles link loss |
| [PackInputs.h](../include/PackInputs.h) | Optional switches and serial commands |
| [Cyclotron.h](../include/Cyclotron.h) | Cyclotron renderers |
| [PackLights.h](../include/PackLights.h) | Ring and power-cell output |
| [PackAudio.h](../include/PackAudio.h), [PackSdAudio.h](../include/PackSdAudio.h), [PackSounds.h](../include/PackSounds.h) | I2S output, SD mixer, sound events |
| [PackWeb.h](../include/PackWeb.h), [PackWebPage.h](../include/PackWebPage.h) | Wi-Fi access point and control page |
| [PackOta.h](../include/PackOta.h), [PackBluetooth.h](../include/PackBluetooth.h), [PackName.h](../include/PackName.h) | Updates over Wi-Fi, Bluetooth speaker, pack name |
