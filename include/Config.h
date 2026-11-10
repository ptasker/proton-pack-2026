#pragma once

#include <stdint.h>

enum class CyclotronMode
{
	Classic1984,
	Afterlife
};

// GPStar system themes (pack/wand sounds, cyclotron style, ramp timings).
// Values are stored in pack flash; keep them stable.
enum class SystemTheme : uint8_t
{
	Gb1984 = 0,
	Gb1989 = 1,
	Afterlife = 2,
	FrozenEmpire = 3
};

namespace config {
// Theme used until one is saved from the web page or USB keys 1-4.
constexpr SystemTheme SYSTEM_THEME = SystemTheme::Afterlife;
constexpr CyclotronMode CYCLOTRON_MODE = CyclotronMode::Afterlife;
constexpr uint8_t RING_PIN = 22;
constexpr uint16_t RING_PIXELS = 40;
constexpr uint8_t RING_BRIGHTNESS = 255;
constexpr uint32_t CYCLOTRON_IDLE_STEP_MS = 95;
constexpr uint32_t CYCLOTRON_FIRING_STEP_MS = 30;
constexpr uint32_t FRAME_INTERVAL_MS = 5;

constexpr uint16_t AFTERLIFE_WINDOW_PIXELS = 6;
constexpr uint16_t AFTERLIFE_WINDOW_OFFSET = 0;
// GPStar Afterlife ramps: steps start/end at i_2021_ramp_delay (300 ms), scaled by
// our 20 ms idle vs GPStar's 7 ms 40-LED idle (x20/7), and speed up over
// i_2021_ramp_length (6 s, quartic-out) on a full startup.
constexpr uint32_t AFTERLIFE_START_STEP_MS = 857;
constexpr uint32_t AFTERLIFE_IDLE_STEP_MS = 20;
constexpr uint32_t AFTERLIFE_FIRING_STEP_MS = 15;
constexpr uint32_t AFTERLIFE_RAMP_MS = 6000;

// GPStar 1984/1989 cyclotron: four windows lit in turn every i_1984_delay
// (CYCLOTRON_DELAY_TVG, 500 ms, used because LED fades are on by default); the
// previous window fades out over 1.5 steps. Ramps start/end at 1.3x the step.
constexpr uint32_t CYCLOTRON_1984_STEP_MS = 500;
constexpr uint32_t CYCLOTRON_1984_START_STEP_MS = CYCLOTRON_1984_STEP_MS * 13 / 10;
constexpr uint32_t CYCLOTRON_1984_RAMP_MS = 3000;    // i_1984_ramp_length, circular-out

constexpr int8_t POWER_BAR_PIN = 23;
constexpr uint16_t POWER_BAR_PIXELS = 15;
constexpr uint8_t POWER_BAR_BRIGHTNESS = 32;
constexpr uint32_t POWER_BAR_STEP_MS = 75;

constexpr int8_t PACK_SWITCH_PIN = -1;
constexpr bool PACK_SWITCH_MOMENTARY = false;
constexpr int8_t FIRE_BUTTON_PIN = -1;
constexpr int8_t VENT_BUTTON_PIN = -1;
constexpr uint16_t DEBOUNCE_MS = 25;
constexpr bool AUTO_START_WITHOUT_SWITCH = true;

constexpr bool WAND_LINK_ENABLED = true;
constexpr int8_t WAND_RX_PIN = 35;
constexpr int8_t WAND_TX_PIN = 25;

constexpr bool AUDIO_BRINGUP_ENABLED = true;
constexpr int8_t AUDIO_BCLK_PIN = 26;
constexpr int8_t AUDIO_LRCLK_PIN = 27;
constexpr int8_t AUDIO_DATA_PIN = 14;
constexpr bool AUDIO_BOOT_BEEP = false;

constexpr bool SD_AUDIO_ENABLED = true;
constexpr int8_t SD_SCK_PIN = 18;
constexpr int8_t SD_MISO_PIN = 19;
constexpr int8_t SD_MOSI_PIN = 13;
constexpr int8_t SD_CS_PIN = 4;
constexpr uint32_t SD_SPI_HZ = 20000000;
// Starting WAV volume, -40..0 dB. 0 dB (full scale) played without resets on
// USB 5V amp power with track 015; lower it if the ESP32 resets on loud peaks.
constexpr int8_t SD_AUDIO_DEFAULT_DB = 0;
// GPStar b_wand_long_startup: true plays the full Afterlife pack startup (015,
// idle loop fading in over 15 s); false plays GPStar's quick startup (001,
// idle loop fading in over 0.5 s). GPStar defaults to quick.
constexpr bool AUDIO_LONG_STARTUP = true;
// GPStar i_wand_idle_level: the pack plays Neutrona Wand idle/beep sounds 20 dB
// below effects volume. Our wand has no speaker, so these are its only source.
constexpr int8_t WAND_SOUND_LEVEL_DB = -20;
// Beep levels, quieter than GPStar by request: the wand arm-switch beep loop (022,
// GPStar -20 dB) and the overheat warning beeps (362, GPStar 0 dB).
constexpr int8_t WAND_BEEP_LEVEL_DB = -24;
constexpr int8_t OVERHEAT_BEEP_LEVEL_DB = -10;
// By request: the higher-pitched "wand ready" sounds with the Vent on (ramp 2 /
// idle 2 and their ramp-downs) play louder than other wand sounds, and the pack
// startup/idle sounds dip by PACK_READY_TRIM_DB while the wand is ready.
constexpr int8_t WAND_READY_LEVEL_DB = -10;
constexpr int8_t PACK_READY_TRIM_DB = -6;
// By request: no first wand hum (141 ramp 1 / 142 idle 1 / 145 ramp-down 1) with
// Activate on and the Vent off; GPStar plays it. Vent-on sounds are unaffected.
constexpr bool WAND_IDLE_1_SOUNDS = false;
// By request: all overheat sounds (vent open/close, steam burst, steam loop and
// fade-out) play below GPStar's 0 dB, and the sequence is simplified (PackSounds.h).
constexpr int8_t OVERHEAT_SOUND_LEVEL_DB = -10;
// Music (tracks 500+, from the web page) plays below full scale for mix headroom.
// By request, while music plays the pack and wand effects (below 500) duck instead.
constexpr int8_t MUSIC_LEVEL_DB = -2;
constexpr int8_t EFFECTS_UNDER_MUSIC_DB = -20;

// Pack Wi-Fi access point with a control page (PackWeb.h). Needs a local
// include/WifiSecrets.h (see WifiSecrets.example.h); without it Wi-Fi stays off.
constexpr bool WIFI_ENABLED = true;
constexpr uint8_t WIFI_CHANNEL = 1;      // GPStar uses channel 1
constexpr uint8_t WIFI_MAX_CLIENTS = 4;
constexpr bool OTA_ENABLED = true;
// Bluetooth speaker mode (A2DP sink) replaces Wi-Fi and pack sounds until the next
// power cycle; lights, wand link, mute and beeps keep working.
constexpr bool BLUETOOTH_ENABLED = true;
constexpr uint32_t MODE_GESTURE_MS = 3000;  // Activate off: Vent on 3 times within this

constexpr uint32_t STARTUP_MS = 1500;
// GPStar Afterlife shutdown: the cyclotron keeps spinning at full brightness while
// slowing over i_2021_ramp_down_length (10.5 s, quartic-in), then fadeOutCyclotron()
// dims it (-10/255 every 50 ms, ~1.3 s). Together they match 348's 11.8 s.
constexpr uint32_t SHUTDOWN_RAMP_MS = 10500;
constexpr uint32_t SHUTDOWN_FADE_MS = 1300;
constexpr uint32_t SHUTDOWN_MS = SHUTDOWN_RAMP_MS + SHUTDOWN_FADE_MS;

constexpr bool themeIsModern(SystemTheme theme)
{
    return theme == SystemTheme::Afterlife || theme == SystemTheme::FrozenEmpire;
}
constexpr CyclotronMode themeCyclotronMode(SystemTheme theme)
{
    return themeIsModern(theme) ? CyclotronMode::Afterlife : CyclotronMode::Classic1984;
}
// GPStar ramp-down lengths: i_1984_ramp_down_length 2.5 s, i_1989_ramp_down_length
// 0.1 s, Afterlife i_2021_ramp_down_length 10.5 s, Frozen Empire a quarter of it.
constexpr uint32_t themeShutdownRampMs(SystemTheme theme)
{
    return theme == SystemTheme::Gb1984 ? 2500 :
           theme == SystemTheme::Gb1989 ? 100 :
           theme == SystemTheme::FrozenEmpire ? SHUTDOWN_RAMP_MS / 4 : SHUTDOWN_RAMP_MS;
}
// Only the modern themes fade the cyclotron out after the ramp (fadeOutCyclotron).
constexpr uint32_t themeShutdownFadeMs(SystemTheme theme)
{
    return themeIsModern(theme) ? SHUTDOWN_FADE_MS : 0;
}
constexpr uint32_t themeShutdownMs(SystemTheme theme)
{
    return themeShutdownRampMs(theme) + themeShutdownFadeMs(theme);
}
inline const char *themeName(SystemTheme theme)
{
    return theme == SystemTheme::Gb1984 ? "1984" : theme == SystemTheme::Gb1989 ? "1989" :
           theme == SystemTheme::FrozenEmpire ? "Frozen Empire" : "Afterlife";
}
constexpr uint32_t MAX_FIRE_MS = 8000;
// GPStar overheat: steam after OVERHEAT_STEAM_DELAY_MS (i_overheating_delay), vent
// closes after level 5 i_ms_overheating_length (6 s) more. Must match the wand.
constexpr uint32_t OVERHEAT_STEAM_DELAY_MS = 4000;
constexpr uint32_t COOLDOWN_MS = OVERHEAT_STEAM_DELAY_MS + 6000;

constexpr bool audioPinAvailable(int8_t pin)
{
    return pin != RING_PIN && pin != POWER_BAR_PIN &&
           pin != PACK_SWITCH_PIN && pin != FIRE_BUTTON_PIN && pin != VENT_BUTTON_PIN &&
           (!WAND_LINK_ENABLED || (pin != WAND_RX_PIN && pin != WAND_TX_PIN));
}

static_assert(!AUDIO_BRINGUP_ENABLED ||
                  (AUDIO_BCLK_PIN == 26 && AUDIO_LRCLK_PIN == 27 && AUDIO_DATA_PIN == 14 &&
                   audioPinAvailable(AUDIO_BCLK_PIN) && audioPinAvailable(AUDIO_LRCLK_PIN) &&
                   audioPinAvailable(AUDIO_DATA_PIN)),
              "MAX98357A bring-up requires unused BCLK26/LRCLK27/DIN14 outputs");
static_assert(!SD_AUDIO_ENABLED ||
                  (AUDIO_BRINGUP_ENABLED &&
                   SD_SCK_PIN == 18 && SD_MISO_PIN == 19 && SD_MOSI_PIN == 13 && SD_CS_PIN == 4 &&
                   audioPinAvailable(SD_SCK_PIN) && audioPinAvailable(SD_MISO_PIN) &&
                   audioPinAvailable(SD_MOSI_PIN) && audioPinAvailable(SD_CS_PIN) &&
                   SD_AUDIO_DEFAULT_DB <= 0 && SD_AUDIO_DEFAULT_DB >= -40),
              "SD audio requires I2S audio, SCK18/MISO19/MOSI13/CS4, and a -40..0 dB start volume");
static_assert(!WAND_LINK_ENABLED ||
                  (WAND_RX_PIN == 35 && WAND_TX_PIN == 25 &&
                   WAND_RX_PIN != RING_PIN && WAND_TX_PIN != RING_PIN &&
                   WAND_RX_PIN != POWER_BAR_PIN && WAND_TX_PIN != POWER_BAR_PIN &&
                   PACK_SWITCH_PIN < 0 && FIRE_BUTTON_PIN < 0 && VENT_BUTTON_PIN < 0),
              "Wand link uses RX35/TX25, distinct LED pins, and no local pack controls");
static_assert(RING_PIXELS > 0, "Cyclotron must contain at least one pixel");
static_assert(static_cast<uint8_t>(SystemTheme::FrozenEmpire) == 3,
              "SystemTheme values are stored in flash and sent to the wand (wand_link::Theme)");
static_assert(themeCyclotronMode(SYSTEM_THEME) == CYCLOTRON_MODE &&
                  themeShutdownMs(SystemTheme::Afterlife) == SHUTDOWN_MS,
              "Default theme must match the default cyclotron mode and shutdown time");
static_assert(CYCLOTRON_IDLE_STEP_MS > 0 && CYCLOTRON_FIRING_STEP_MS > 0 && FRAME_INTERVAL_MS > 0,
			  "Animation intervals must be positive");
static_assert(AFTERLIFE_WINDOW_PIXELS > 0 && AFTERLIFE_WINDOW_PIXELS <= RING_PIXELS / 4,
			  "Four Afterlife windows must fit without overlapping");
static_assert(AFTERLIFE_WINDOW_OFFSET < RING_PIXELS,
			  "Afterlife window offset must be within the ring");
static_assert(AFTERLIFE_START_STEP_MS > 1 && AFTERLIFE_IDLE_STEP_MS > 0 &&
				  AFTERLIFE_IDLE_STEP_MS <= AFTERLIFE_START_STEP_MS &&
				  AFTERLIFE_FIRING_STEP_MS > 0 &&
				  AFTERLIFE_FIRING_STEP_MS <= AFTERLIFE_START_STEP_MS && AFTERLIFE_RAMP_MS > 0,
			  "Afterlife step and ramp intervals must be valid");
static_assert((AFTERLIFE_IDLE_STEP_MS * 3 + 3) / 7 >= FRAME_INTERVAL_MS,
              "Fastest staged Afterlife step must allow at least one rendered frame");
static_assert(POWER_BAR_PIXELS > 0 && POWER_BAR_STEP_MS > 0,
			  "Power bar size and interval must be positive");
static_assert(STARTUP_MS > 0 && SHUTDOWN_RAMP_MS > 0 && SHUTDOWN_FADE_MS > 0 &&
                  MAX_FIRE_MS > 0 && COOLDOWN_MS > 0,
			  "Pack state durations must be positive");
}
