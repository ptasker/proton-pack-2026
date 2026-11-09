# Wi-Fi page, updates and Bluetooth

## Wi-Fi control page

The pack runs its own Wi-Fi network (it has no internet) and serves a control
page from it.

### Setup

1. Copy `include/WifiSecrets.example.h` to `include/WifiSecrets.h` and set
   `AP_PASSWORD` (8-63 characters). The file is git-ignored. Without it the
   pack builds with Wi-Fi off and says so at boot.
2. Build and upload `esp32dev`. The boot log prints the network name and URL.
3. Join the network on your phone and open **http://192.168.4.1/**.

The network is named `ProtonPack_XXXX`, from the last bytes of the board's MAC
address, until you set a pack name. `WIFI_ENABLED`, `WIFI_CHANNEL` and
`WIFI_MAX_CLIENTS` are in [include/Config.h](../include/Config.h).

### What the page does

- **Status**, refreshed twice a second: pack state, wand link and switches, SD
  card, Wi-Fi clients, audio underruns and uptime.
- **Theme**: choose one of the four themes. Saved.
- **Mute**: the latest change wins, whether from the page or the wand gesture.
- **Volume**: master volume, -40 to 0 dB. Not saved across power-off.
- **Music**: play or stop tracks numbered 500 and up. Effects duck while music
  plays.
- **Pack name**: one name for the Wi-Fi network and the Bluetooth speaker,
  1-24 letters, digits, spaces, `-`, `_` or `.`. Blank restores the default.
  Saving restarts the pack; rejoin the renamed network with the same password.
- **Bluetooth speaker**: restart in Bluetooth mode.

### HTTP API

| Request | Effect |
| --- | --- |
| `GET /` | Control page |
| `GET /api/status` | Status JSON |
| `GET /api/music` | Music tracks, `[{"n":500,"name":"500_x.wav"}]` |
| `POST /api/theme?t=2` | Theme: 0 = 1984, 1 = 1989, 2 = Afterlife, 3 = Frozen Empire |
| `POST /api/mute?on=1` | Mute (`on=0` unmutes) |
| `POST /api/volume?db=-12` | Master volume, -40 to 0 dB |
| `POST /api/music/play?track=500` | Play a music track |
| `POST /api/music/stop` | Stop music |
| `POST /api/name?n=My Pack` | Set the pack name and restart |
| `POST /api/bluetooth` | Restart as a Bluetooth speaker |

There is no login on the page or API: anyone on the pack's Wi-Fi network can
use them, so the Wi-Fi password is the only protection.

## Firmware updates over Wi-Fi

After one USB upload, the pack accepts PlatformIO uploads over its own network
(ArduinoOTA on port 3232). Join the pack's Wi-Fi, then:

```sh
pio run -e pack_ota -t upload
```

- The update password is the Wi-Fi password.
  [scripts/ota_auth.py](../scripts/ota_auth.py) reads it from `WifiSecrets.h`,
  so keep it a plain quoted string with no backslashes.
- During an upload the pack stops its sounds and lights, and the wand sees the
  link drop.
- The new image goes to a spare partition and is only used once verified. If
  the upload fails, the old firmware keeps running.
- Allow incoming connections if your computer's firewall asks; the pack
  connects back to it.
- The wand always uploads over USB.

## Bluetooth speaker mode

The pack can restart as a Bluetooth speaker. Pair from the phone's Bluetooth
settings; no PIN is needed. One phone connects at a time, and the phone's
volume sets the level.

| | How |
| --- | --- |
| Enter | "Switch to Bluetooth" on the web page, USB `b`, or the wand gesture |
| Leave | Power-cycle the pack, USB `b`, or the wand gesture |
| Wand gesture | With Activate off, flip Vent on and off three times within 3 seconds |

In Bluetooth mode **Wi-Fi, the web page, updates over Wi-Fi and pack sounds are
off**, because the Bluetooth stack needs the memory the SD mixer uses. Lights,
the wand link, mute and confirmation beeps still work.

Any power-on, upload or crash boots back to Wi-Fi mode; Bluetooth mode only
survives the software restart that enters it.

Pairing needs no confirmation, so anyone in range can connect while the pack is
in this mode and no phone is connected.

## Memory

RAM is the tight resource on the ESP32-WROOM.

- The SD card mounts first, then the voice buffers, then Wi-Fi. The boot log
  prints free heap after each stage; roughly 30-40 KB remains with Wi-Fi and
  updates running. Check it before growing any buffer.
- The web server's handlers only queue commands and copy status snapshots, so
  they never touch pack state directly. They run on core 1, away from audio.
- The pack uses the `min_spiffs.csv` partition table (two 1.9 MB app slots) to
  fit the Bluetooth stack and still allow updates over Wi-Fi. Changing the
  partition table needs one USB upload.
