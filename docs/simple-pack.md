# Simple pack

A second, simpler pack on a single ESP32-WROOM with no separate wand board. It
is its own build (`simple_pack`, [src/simple_pack.cpp](../src/simple_pack.cpp)
and [include/simple/](../include/simple/)) and reuses the main pack's
controller, sounds and themes unchanged, so it has the same timing, firing
limit, overheat and mute gesture.

## Hardware

| Item | GPIO |
| --- | --- |
| Cyclotron: four 7-pixel jewels (28 pixels) | 22 |
| Power cell: 12-pixel strip | 23 |
| Wand chain (10 pixels): 4-pixel bargraph, two body lights, vent, 3-pixel firing strip | 21 |
| Activate switch (to GND) | 33 |
| Fire button (to GND) | 32 |
| Amp BCLK / LRCLK / DIN | 26 / 27 / 14 |
| SD SCK / MISO / MOSI / CS | 18 / 19 / 13 / 4 |

Settings are in [include/simple/SimpleConfig.h](../include/simple/SimpleConfig.h).

## Behaviour

- **Activate** starts the pack and wand together. There are no Vent or wand
  switches; both follow Activate.
- **Fire** works after the 1.5-second start-up, on a fresh press.
- Each cyclotron jewel shows one of the main pack's lens windows on its outer
  ring, with the centre pixel following the brightest one.
  `CYCLOTRON_GAIN_PERCENT` brightens the main pack's dim red.
- The 4-pixel bargraph follows the theme, dimmed to 25%
  (`BARGRAPH_LEVEL_PERCENT`).
- The armed-wand beep loop (`ARM_BEEPS`) and the Afterlife wand-ready hum
  (`WAND_READY_HUM`) are off by default.
- Master volume defaults to -6 dB (`DEFAULT_VOLUME_DB`) and, unlike the main
  pack, is saved when changed.

## Wi-Fi page

The simple pack runs its own `ProtonPack_XXXX` network, using the password from
`include/WifiSecrets.h`, with a control page at **http://192.168.5.1/**: theme,
mute, volume and music. USB keys `1`-`4` also change the theme. There is no
Bluetooth mode and no pack-name setting.

It uses 192.168.5.1 rather than the main pack's 192.168.4.1 so that an update
over Wi-Fi cannot reach the wrong pack.

## Uploading

```sh
pio run -e simple_pack -t upload --upload-port <port>
```

[scripts/require_mac.py](../scripts/require_mac.py) refuses the upload unless
the connected board's MAC address matches `custom_required_mac` in
`platformio.ini`. It exists so the simple-pack build cannot overwrite the main
pack by mistake. **Change that value to your own board's MAC**, or remove the
line to skip the check.

For updates over Wi-Fi, join the simple pack's network and run:

```sh
pio run -e simple_pack_ota -t upload
```

The boot log's update hint names the main pack's `pack_ota` environment; use
`simple_pack_ota` here.

## Power

Holding fire caused brownout resets when the ESP32 shared the amp's 5V battery
feed. The web page's System line shows the last restart reason. Give the ESP32
its own 5V supply or regulator, and run the amp and pixels on separate thick
wires from the battery with all grounds joined.

After a brownout the firmware releases a stuck SD card and retries the mount,
so sound comes back without a power cycle.
