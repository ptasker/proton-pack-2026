# Project Instructions

## Fast Iteration

- Prefer small, targeted changes and concise updates. Optimize for quick hardware
  iteration rather than exhaustive verification on every change.
- The user runs builds, tests, uploads, and serial checks. Do not run these unless
  explicitly asked. Provide the relevant commands instead, using
  `~/.platformio/penv/bin/pio` on this Mac, and distinguish required steps from
  optional checks. Do not claim unrun checks passed.
- Keep tests proportional to the change. Reuse existing tests and add focused
  coverage only for meaningful risks; routine settings or effect tuning do not
  need new test suites.
- During the current C3 wand bring-up, do not add or run tests unless asked.
  Preserve the existing pack tests.
- A fully passing test suite is not a prerequisite for iteration. Report known
  relevant failures briefly, leave unrelated failures alone, and do not delete
  or weaken tests just to obtain a pass. Preserve hardware safety guards.

## Current Hardware and Scope

- The ESP32-C3 Super Mini wand (`wand_c3`) now controls the ESP32-WROOM pack
  (`esp32dev`) over a limited UART link. Preserve existing pack lighting and tests.
- GPIO0 is Activate, GPIO1 is Vent, GPIO20 is switch 3 (wand enable), GPIO2 is the
  fire button, and GPIO21 drives one 34-pixel NeoPixel chain (RGB/GRB assumed).
  All three switches are installed (Activate = left toggle, Vent = top right,
  wand enable = lower right; user-confirmed 2026-10-08). 2026-10-13: the Vent
  switch stopped responding after shell install (hot-glued, not debugged), so
  `VENT_SWITCH_PIN = -1` with `VENT_FOLLOWS_WAND_SWITCH = true`: the lower-right
  switch now drives Vent + wand enable (vent jewel, Clippard, sounds, firing,
  Bluetooth gesture). Restore pin 1 / false when the switch is fixed. Firmware expects
  active-LOW contacts to GND; GPIO2 is a strapping pin, so leave fire released
  during reset/upload. GPIO18/19 are reserved for native USB.
- Current mode uses `LED_DIAGNOSTIC_MODE = false` and
  `BENCH_IDLE_FIRING_MODE = false`: on 2026-09-23 the user reported corrected
  wiring and requested all three physical switches re-enabled. Read Activate
  on GPIO0, Vent on GPIO1, wand enable on GPIO20, and fire on GPIO2. Both
  missing-switch bypass options are `false`. Keep the Afterlife-style hats and
  fixed firing preset; no rotary encoder is installed.
- Optional `BENCH_IDLE_FIRING_MODE = true` skips all switch initialization and
  polling, assumes all on, and keeps the physical fire button. It starts idle
  immediately and uses idle lighting during cooldown. Do not re-enable this
  bypass unless requested.
- Preserve the fresh-press guard, 8-second firing limit, and 10-second overheat
  cooldown (lengthened from 3 s on 2026-10-07 for GPStar overheat sounds;
  `COOLDOWN_MS` must match in WandConfig.h and Config.h).
  A held button must not fire on startup, rearming, or cooldown completion. Do
  not queue presses made during lockout. Normal switch mode retains its
  1.5-second startup; Activate off clears lights and either firing gate off
  stops firing.
- LED-only diagnostic remains available with `LED_DIAGNOSTIC_MODE = true`,
  taking priority over bench mode: ignore all inputs and run the white pixel
  scan then full-chain red/green/blue/white/off at brightness 16/255.
- Normal mode (both flags false) uses GPStar's user-selected Super Hero staged
  switches and is currently active. Real assigned inputs take precedence over
  missing-switch bypasses.
- LED order: slo-blo 0, vent jewel 1-7, hats 8-9, Clippard 10, firing strip
  11-33. Keep firing effects out of the indicator sections. See
  [WandConfig.h](../include/WandConfig.h) for settings.
- UART: C3 GPIO6 RX (green) connects to pack GPIO25 TX; C3 GPIO7 TX (blue)
  connects to pack GPIO35 RX, with common ground and 3.3V logic. Pack GPIO34/35
  are input-only. Both link flags default true; pack auto-start and USB p/f/s/v
  controls are bypassed in link mode. Status uses 9600 8N1, CRC, 100 ms heartbeats,
  and a 500 ms receive timeout. Link loss stops pack firing and shuts it down;
  reconnect requires released fire while ready and a new press. Green-wire
  acknowledgements are diagnostic only; wand lighting remains independent.
- The pack sends a `PackStatus` frame (type 3, theme 0-3) every 500 ms and on
  change; the wand saves it in NVS `wand/theme`. The wand bargraph follows the
  theme: 1984/1989 use GPStar Super Hero idle sweeps, Afterlife/Frozen Empire
  the Original slow fill-and-hold; all use Super Hero firing (15 ms, faster
  near overheat) and 80 ms top-down drains for overheat and Activate off.
- UART protocol v2 adds an absolute speed level (1-4) from the wand overheat
  timer; both boards must use the same version. Levels rise below 1/2, 1/3, and
  1/4 remaining firing time and reset outside firing. Pack Afterlife uses
  independent circular LED fades, quartic startup/shutdown speed ramps, and
  staged 20/14/11/9 ms motion through the four six-pixel lens groups.
  Keep the existing dim warm-red output. Afterlife startup/shutdown follow
  GPStar timing to match the SD sounds: 857 -> 20 ms quartic-out over 6 s
  (GPStar 300 ms ramp delay scaled x20/7), and shutdown = 10.5 s quartic-in
  slowdown at full brightness + 1.3 s fade (`SHUTDOWN_MS` 11.8 s = track 348).
  The power cell keeps cycling during ramps, each step slowed by the current
  cyclotron step (GPStar powercellLoop).
- Pack cyclotron uses 40 pixels on GPIO22; power cell uses 15 on GPIO23.
- Normal `esp32dev` MAX98357A audio bring-up uses I2S0: BCLK GPIO26, LRCLK GPIO27,
  DIN GPIO14. It sends silence by default; the optional 200 ms 1 kHz boot beep
  (`AUDIO_BOOT_BEEP`) is now off. SD WAV volume defaults to 0 dB. USB `t`
  requests one low-level one-second tone, `m` stops all audio.
  Automatic sounds: GPStar Afterlife pack startup/shutdown only (see below).
  Keep the amplifier SD/SD_MODE pin grounded for hardware mute during uploads
  and resets; its pull-up arrangement is unverified, so no direct ESP32 GPIO
  enable connection is approved. External amp 5V must not feed the USB-powered
  ESP32's power pins. The C3 has no audio output.
- The 3.3V SPI SD reader uses pack SCK18/MISO19/MOSI13/CS4. `PackSdAudio.h`
  mounts it, indexes GPStar-numbered `NNN_*.wav` files in the root (number =
  GPStar sound enum index), and mixes up to 8 16-bit 44.1 kHz tracks (mono
  downmix) with per-track loop, dB gain and linear-dB fades from -70 dB, like
  GPStar Audio's play/stop/fade. `PackSounds.h` follows GPStar Afterlife
  packStartup/packShutdown: Starting plays 015 + 016 idle loop fading in over
  15 s (`AUDIO_LONG_STARTUP`; quick mode = 001 + 0.5 s fade); Stopping plays 348,
  stops 001/387/015, fades 016 out over 50 ms. USB `#NNN`+Enter, `d`, `+`/`-`,
  `m`. Firing: 155 + 098 loop (0.3 s fade), tails 385/386/387 by length on a
  15 s timer, no tail on shutdown. The pack also plays the speakerless wand's
  sounds from UART switch flags (GPStar extra pack sounds, -20 dB
  `WAND_SOUND_LEVEL_DB`): Activate 141 -> 142 loop; Vent on 143/147 -> 144 loop;
  Vent off 146 -> 142; arm switch 022 beep loop (ends naturally while firing,
  restarts 1 s after); Activate off 017 + 148/145. Overheat (= our Cooling):
  362 warning beep loop at speed level 4 (stopped with firing); at Cooling 360,
  361 loop, 341 (+ wand 148, 017, beep ends); +4 s 178, random 061/356-359
  (0.12 s fade), 179 loop (1 s fade); end 355 + 180, stop 179/361; wand ramp/idle
  and beep resume. Pack overheat lights unchanged (25% dim for 10 s).
  Beeps are quieter than GPStar by request: 022 at `WAND_BEEP_LEVEL_DB` (-24),
  362 at `OVERHEAT_BEEP_LEVEL_DB` (-10).
  Vent-on "wand ready" sounds (143/147/144/146/148) at `WAND_READY_LEVEL_DB`
  (-10); pack 015/016 dip by `PACK_READY_TRIM_DB` (-6) via mixer `setTrim()`
  (~1 s slide, independent of fades) while the Vent is on (not during overheat).
  By request `WAND_IDLE_1_SOUNDS = false` (no 141/142/145 first wand hum with
  Vent off). Overheat simplified by request (only 341 vent open + tail; steam
  burst + 179 at +4 s; 355 + 180 at end; 144 fades out; no 360/361/178/148/017)
  at `OVERHEAT_SOUND_LEVEL_DB` (-10): many simultaneous file opens glitched audio.
- Sound mute (2026-10-07): hold fire 3 s (`MUTE_HOLD_MS`) with Activate off,
  fresh press required; wand `WandMute.h` saves it in NVS and sends link flag bit
  7 (`Muted`; validFlags no longer rejects it). Pack fades the SD mix only
  (~60 ms) and beeps low 500 Hz (mute) / high 1 kHz (unmute) on changes, not at
  connect; keeps the last state on link loss. Both boards must be updated together.
- System themes (2026-10-07): `SystemTheme` 1984/1989/Afterlife/Frozen Empire in
  Config.h, owned by PackController (cyclotron mode derived; setCyclotronMode kept
  for tests), saved in pack NVS `pack/theme`, chosen via web page or USB 1-4.
  Per-theme pack tracks in `themeSounds()` (PackSounds.h); 80s wand path
  (329/333 on Vent, 017 off, single 022 beep per arm). `Cyclotron1984` renders
  1984/1989 (4 windows, 500 ms, 1.5-step fade, circular ramps 3 s / 2.5 s or 0.1 s,
  no fade-out); Frozen Empire uses the Afterlife renderer with a 2.6 s ramp-down.
  Shutdown length is `themeShutdownMs()`. All 52 theme tracks verified on card.
- Music (500+) plays at `MUSIC_LEVEL_DB` (-2); by request, while any music plays
  the effects (below 500) duck `EFFECTS_UNDER_MUSIC_DB` (-20) via `setEffectsTrim()`. Card has 500 (GPStar's licensed
  track) plus user-added songs; never supply copyrighted audio.
- Pack Wi-Fi (2026-10-07, step 1): own AP via `PackWeb.h` + `PackWebPage.h`
  (ESPAsyncWebServer 3.x on Arduino 2.0.17, AsyncTCP on core 1). Password lives in
  git-ignored `include/WifiSecrets.h` (template `WifiSecrets.example.h`); never
  commit it or print it. Page at 192.168.4.1: status, mute (latest change wins with
  the wand gesture), master volume, music 500+ play/stop. Handlers only queue
  commands / copy loop-published JSON. Next: persist settings, password change.
  Bluetooth speaker mode (2026-10-08, working with iPhone): `PackBluetooth.h`
  (IDF Bluedroid A2DP sink + AVRCP), entered by web button, USB `b`, or wand
  gesture (Activate off, Vent = top right toggle, 3 flips in 3 s); leaves by
  power cycle or the same gesture/key. Wi-Fi, OTA and SD pack sounds are OFF in
  this mode (user approved; pairing needs that RAM). Partitions `min_spiffs.csv`.
  Pack OTA: `PackOta.h` (ArduinoOTA, no mDNS, port 3232, Wi-Fi password via
  `scripts/ota_auth.py`); upload with `pio run -e pack_ota -t upload` while on
  the pack AP. onStart stops SD voices and blanks lights. Flash use with Wi-Fi ~909 KB of the 1.3 MB app partition.
  RAM is tight: SD mounts first (FATFS ~4.6 KB per open file, 8 files, one
  contiguous block), then 8 x 8 KB voice rings, then Wi-Fi; ~43 KB heap remains.
  AsyncTCP stack 8 KB; packed track-name buffer; reader opens one file per pass.
  Boot prints heap after each stage. Don't grow buffers without checking it.
  Accepted: a few dozen inaudible underruns per session at Vent on and pack
  shutdown (several file opens at once); user cleaned macOS `._` files from the
  card. Not volume-related. Revisit only if ticks become audible.
  Accepted (2026-10-08): faint fizz on the 1984/FE hums (379/380/413/405) is in
  the recordings (69 Hz hum + low-level mid grit exposed by the small speaker);
  playback chain verified clean. Don't re-diagnose; a larger speaker would help.
  SD reader: core 0, priority 2, POSIX read() up to 8 KB (stdio + core-1 sharing
  caused underrun static). I2S task: core 0, priority 3, with a block peak limiter
  (ceiling 32000, ~0.3 s release) that removed firing-mix clipping static. The
  log reports underruns and residual clipping; both were zero on 2026-10-06.
  Smoke, vibration, and barrel sensor remain deferred.
- On 2026-10-06 15:49, normal `esp32dev` firmware (with boot beep) was restored
  on the new ESP32, replacing `pack_audio_test`. The notes below describe that
  diagnostic, which remains buildable but is not installed.
- On 2026-10-06, the main pack was uploaded with temporary `pack_audio_test`
  firmware to isolate persistent tone crackling after amplifier and speaker
  swaps. The current uploaded revision generates I2S samples directly in
  `loop()`, with eight 256-frame DMA buffers and no separate audio task.
  It boots silently; USB `t` starts a continuous 1 kHz tone at peak 8000/32767
  (about -12 dBFS), and `m` stops it. Unlike the normal pack tone, this is louder
  and does not stop automatically. `?` reports status and settings.
  The diagnostic clears the cyclotron and sets all 15 power-cell
  pixels steady blue at brightness 32/255 once, with no ongoing LED updates.
  USB `l` resends the static lights after reconnecting/waking LED power while
  the tone is stopped. Battery keep-awake behavior is unverified.
  There are no animations, pack controls, or wand UART acknowledgements.
  C3 is unchanged.
  Restore normal pack operation by uploading `esp32dev` when requested. The amp
  has no power switch: disconnect its battery/external supply at the connector
  for uploads/resets. Normal `PackAudio` retains the -30 dBFS one-second tone;
  do not reuse its old one-shot serial script for the continuous diagnostic
  without an explicit `m` stop, including on host-side errors after starting it.
- 2026-10-06 15:41: main ESP32 replaced (new MAC 70:4b:ca:7c:ba:80). With the amp
  powered from the ESP32's 5V pin (USB VBUS) instead of the external battery,
  the 1 kHz test tone played cleanly with no crackling. The external battery
  supply is the prime suspect for earlier crackling/silence; USB 5V is a bench
  arrangement only. Never connect battery 5V to the ESP32 5V pin while USB is
  also connected.

## Simple Pack (Second Build, Bring-Up)

- A simplified second pack runs on a third ESP32-WROOM (old
  audio-test board, MAC 04:b2:47:92:12:8c) on a Freenove ESP32 breakout. Check
  the MAC before uploading so the main pack (70:4b:ca:7c:ba:80) is never
  overwritten. No C3: the pack ESP32 will read the wand's 2 switches + fire
  button directly and drive all pixels. Keep main pack/wand builds untouched.
- Confirmed: 4 x 7-pixel jewels (28 NeoPixels, GRB, chained) on **GPIO22**
  (user-confirmed 2026-10-16; presumably the cyclotron).
- Final hardware (user, 2026-10-17; GPIOs not yet connected): cyclotron 4 x 7
  jewels (28); power cell 12-pixel strip; wand one chain of 10: 4-pixel strip,
  3 single pixels (2 body indicators + 1 vent light), 3-pixel firing strip.
  Inputs: one Activate switch and one fire button only (no vent/arm gates).
- Planned pins (main-pack compatible): jewels 22 (confirmed), power cell 23,
  wand chain 21, Activate 33, fire 32 (as wired; both to GND, internal
  pull-ups; verified with the input test 2026-10-17), audio
  26/27/14, SD 18/19/13/4. Spare: 25, 16, 17.
- `simple_pack` env (2026-10-17, working): `src/simple_pack.cpp` + `include/simple/`
  (SimpleConfig, SimpleWand, SimpleLights). Kept separate from the main pack: it
  includes the main PackController/PackSounds/PackAudio/PackSdAudio/Cyclotron/
  WandController/WandMute/Config.h/WandConfig.h unchanged (never edit them for the
  simple pack). Vent/arm follow Activate; flags drive the pack like
  PackWandReceiver::apply. Jewels show the main renderers' 6-pixel lens windows
  (centre = brightest), `CYCLOTRON_GAIN_PERCENT` 300 by request. Wand: 4-pixel
  theme bargraph, blue body pixel (index 4, blue lens), white body, vent, 3-pixel
  firing strip. Arm beeps off (`ARM_BEEPS = false`) and the Afterlife/FE wand-ready
  whine (147/144) off (`WAND_READY_HUM = false`), both by request. Wi-Fi page (SimpleWeb/SimpleWebPage: theme,
  mute, volume, music) on its own AP (`ProtonPack_XXXX`) at **192.168.5.1** (main
  pack is 192.168.4.1, so OTA can't hit the wrong pack); OTA via
  `pio run -e simple_pack_ota -t upload`. No Bluetooth.
  2026-10-20: holding fire caused BROWNOUT resets (confirmed via the page's
  "Last restart"; amp + pixels on battery 5V). Boot now releases a stuck SD card
  (CS high + clocks) and retries the mount 3x, so sound returns after a brownout.
  Web System line shows last restart reason and restarts since power-on.
  Mitigations by request: default volume -6 dB (`DEFAULT_VOLUME_DB`), web/USB
  volume saved in NVS `pack/volume`, bargraph at 25 % (`BARGRAPH_LEVEL_PERCENT`);
  user is moving the ESP32 to its own supply/regulator, separate from the amp.
  `scripts/require_mac.py` blocks uploads unless MAC 04:b2:47:92:12:8c.
- `simple_pixel_test` env (`src/simple_pixel_test.cpp`): blinks GPIO22 five times
  (pin finder), then scans/fills a chain. 2026-10-16: 4 x 7-pixel jewels (28) on
  GPIO22 verified, GRB order, chain order correct.
- GPIO21 verified (jewels and a 12-pixel strip light on it). The 3 single + 3-strip
  wand pixels stayed dark on GPIO21 even with correct wiring and the first pixel
  removed; likely 3.3V data too weak for them (needs a 74AHCT125 level shifter),
  or a different pixel type. Parked by the user 2026-10-16.
- Freenove breakout: its 5V pins come from the barrel-jack regulator, not USB;
  pixels stay dark on USB power alone. The red header by the jack is the 7-12V
  input; never feed pixels from it.

## Check GPStar Before Updates

- Before updating the wand or adding new behavior, consult the local checkout
  of [GPStar Core Source](https://github.com/GPStarTechnologies/GPStar-Core-Source)
  at `reference/GPStar-Core-Source/`, especially `source/NeutronaWand/` and
  `source/SharedLib/`. Use `source/ProtonPack/` when pack migration begins.
- GPStar is the primary upstream reference, replacing GBPack32 for new work.
  The legacy pack remains GBPack32-inspired; consult `reference/GBPack32/`
  only when maintaining that existing implementation.
- Read and search local files and Git history instead of making per-file web or
  API requests. The checkout is Git-ignored, so include ignored files when
  searching within this specific directory.
- Use the existing local snapshot without automatic refreshes or remote GitHub
  checks. Only access GitHub when the user explicitly requests an upstream
  refresh, publishing, or another remote operation.
- When a refresh is explicitly requested, first check
  `git -C reference/GPStar-Core-Source status --short`. If clean,
  run `git -C reference/GPStar-Core-Source pull --ff-only origin main` from the project root.
  Never discard local reference changes or force-reset a diverged checkout.
- Read only the relevant upstream implementation and local history. Use its
  behavior and design as the default reference instead of inventing a replacement
  or assuming an earlier local approximation matches it.
- Adapt to this project's confirmed hardware, pixel counts, wiring, and available
  components. Explain intentional differences from upstream before implementing them.
- Do not blindly merge upstream changes. Confirm applicable reuse permission or
  licensing before copying source, and preserve required attribution. GPStar
  is GPL-3.0-or-later; direct source imports must comply with its license. The
  current C3 firmware is a limited behavioral adaptation, not a full source port.
- Treat the checkout as reference-only: do not edit its source, include it in
  firmware builds, or commit it into this project's repository.
- If a refresh fails, use the existing local checkout and report that upstream
  freshness could not be verified. If no checkout is available either, say so.
- When reporting an update, briefly identify the upstream behavior or changes
  checked and any differences retained in this project.
