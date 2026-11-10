# Audio

The pack plays WAV files from a microSD card through a MAX98357A I2S amplifier.
A GPStar pack sends play, loop and fade commands to a separate audio board;
here the ESP32 does that board's job itself with a small software mixer.

## SD card

- Format: uncompressed 16-bit PCM, 44.1 kHz, mono or stereo. Stereo is mixed
  to mono because the amp drives one speaker.
- Files go in the card root, named the GPStar way: a three-digit number, an
  underscore, a name, `.wav`. For example `015_afterlife_pack_startup.wav`.
- The number is the GPStar sound index. Tracks 001-499 are effects; 500 and up
  are music.
- Other files and folders are ignored. The boot log reports the track count,
  duplicates and gaps.

No audio is included in this repository; supply your own files. On a Mac, run
`dot_clean -m /Volumes/<card>` before ejecting: the hidden `._` files macOS
adds slow down file opens.

To add music, name files `501_name.wav`, `502_...` with no gaps in the
numbering and restart the pack.

## Themes

Choose a theme on the web page or with USB keys `1`-`4`. The choice is saved in
flash; `SYSTEM_THEME` in `Config.h` is the default until one is saved. Switching
while the pack is on crossfades the hum over half a second and restarts the
cyclotron.

| | 1984 | 1989 | Afterlife | Frozen Empire |
| --- | --- | --- | --- | --- |
| Pack start-up | 379 + 380 hum (3.8 s fade-in) | 037 + 038 hum (3.8 s) | 015 + 016 (15 s) | 413 + 405 hum (10 s) |
| Pack shutdown | 035 | 039 | 348 | 418 |
| Fire start / loop | 391 / 393 | 097 / 098 | 155 / 098 | 335 / 098 |
| Tail: short / mid / long | 394 / 395 / 392 | 010 / 071 / 011 | 385 / 386 / 387 | always 386 |
| Overheat warning / vent close | 060 / 342 | 060 / 342 | 362 / 355 | 362 / 355 |
| Wand sounds | Vent on 329, off 017, one 022 beep per arm | Same, Vent on 333 | Ramp, idle and beep loops (below) | Same as Afterlife |

`AUDIO_LONG_STARTUP` chooses between the full Afterlife and Frozen Empire
start-up (default) and GPStar's quick start-up (track 001 with a half-second
fade-in). 1984 and 1989 have a single start-up.

## What plays when (Afterlife)

### Pack

| Event | Sounds |
| --- | --- |
| Activate on | 015 start-up, with the 016 idle loop fading in over 15 s |
| Activate off, or link lost | 348 shutdown; the idle loop fades out |
| Firing starts | 155 fire start, with the 098 firing loop fading in over 0.3 s |
| Firing stops | A tail by firing length: 385 under 5 s, 386 for 5-10 s. No tail if the pack is shutting down |

The idle loop keeps playing through firing and cooldown.

### Wand

The wand has no speaker, so the pack plays its sounds from the switch states it
receives.

| Event | Sounds |
| --- | --- |
| Vent on | 143 ramp (147 if Vent was already on at Activate), then the 144 idle loop |
| Vent off | 146 ramp-down |
| Wand switch on, with Vent on | 022 beep loop |
| Wand switch off | Beep loop stops |
| Firing | Beep loop finishes its pass, then restarts 1 s after firing ends |
| Activate off | 017 wand shutdown, plus the 148 ramp-down if Vent was on |

### Overheat

| When | Sounds |
| --- | --- |
| Last quarter of the firing limit | 362 warning beeps, looping |
| Overheat (8 s of firing) | Firing tail, then 341 vent open. The wand idle hum fades out |
| 4 s later | A random steam burst (061, 356-359) and the 179 steam loop |
| 10 s (cooldown ends) | 355 vent close and 180 steam fade-out. Wand sounds resume |

## Differences from GPStar

These are deliberate, and each is a setting in `Config.h`.

| Setting | Value | Effect |
| --- | --- | --- |
| `WAND_IDLE_1_SOUNDS` | `false` | No wand hum with Activate on and Vent off (GPStar plays 141/142) |
| `WAND_SOUND_LEVEL_DB` | -20 | Level for wand idle and ramp sounds, as GPStar |
| `WAND_READY_LEVEL_DB` | -10 | The Vent-on "wand ready" sounds are louder than GPStar's -20 dB |
| `WAND_BEEP_LEVEL_DB` | -24 | Arm beep loop, 4 dB quieter than GPStar |
| `PACK_READY_TRIM_DB` | -6 | The pack hum dips while the Vent is on |
| `OVERHEAT_BEEP_LEVEL_DB`, `OVERHEAT_SOUND_LEVEL_DB` | -10 | Overheat sounds are quieter than GPStar's 0 dB |
| `MUSIC_LEVEL_DB` | -2 | Music and Bluetooth audio level |
| `EFFECTS_UNDER_MUSIC_DB` | -20 | Effects duck by this much while music plays |

The overheat sequence is also trimmed: GPStar plays several more files at the
moment of overheat, and opening that many at once glitched the audio.

## Mixer

- Up to **8 tracks at once**, each with its own loop flag, gain and fades.
  Fades are linear in dB down to -70 dB, which is treated as silence. A stop is
  a 5 ms fade to avoid clicks.
- Master volume runs from -40 to 0 dB in 3 dB steps (`+` / `-`, or the web
  page), starting at `SD_AUDIO_DEFAULT_DB`. For more than 0 dB, change the
  amp's hardware gain.
- A peak limiter keeps overlapping loud tracks from clipping. It checks each
  2.9 ms block and recovers over about 0.3 seconds.
- Mute fades the SD mix out over about 60 ms. Tracks keep running silently, so
  unmuting brings back whatever loop is playing. Confirmation beeps and the
  test tone ignore the mute.
- The pack runs normally without a card.

### Internals

Each voice streams through its own 8 KB buffer and starts once about 46 ms is
buffered. A reader task (core 0, priority 2) reads files with POSIX `read()`;
the I2S output task (core 0, priority 3) is never delayed by SD reads. The
Arduino loop, the web server and the lights run on core 1.

The log reports buffer underruns, clipped samples and limiter activity once per
second, and the web page shows the same counts.

### Known limits

- Starting many sounds at the same moment can cause a few underruns, because
  each file open scans the card's root directory. The reader opens one new
  file per pass to limit this. A few dozen inaudible underruns per session, at
  Vent on and at shutdown, are expected.
- The 1984 and Frozen Empire hums have a faint fizz on a small speaker. It is
  in the recordings: most of their energy is a 69 Hz hum that a small speaker
  cannot reproduce, which leaves the low-level grit exposed.
- The main pack's volume is not saved across power-off.

## Test tone

USB `t` plays a one-second 440 Hz tone at about -30 dBFS; `m` stops all audio.
Both work with the pack off. `AUDIO_BOOT_BEEP = true` adds a short 1 kHz beep
at boot.

A low digital level does not guarantee a quiet speaker. Keep the speaker away
from your ears when testing an unknown amp gain.
