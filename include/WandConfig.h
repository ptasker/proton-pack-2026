#pragma once

#include <stdint.h>

namespace wand_config
{
    constexpr bool LED_DIAGNOSTIC_MODE = false;
    constexpr bool BENCH_IDLE_FIRING_MODE = false;
    constexpr bool BARGRAPH_DIAGNOSTIC_MODE = false;

    constexpr int8_t ACTIVATE_SWITCH_PIN = 0;
    // Vent switch on GPIO1 stopped responding after shell install (2026-10-13; not
    // debugged, hot-glued). Vent now follows the wand switch (lower right), which
    // drives the vent jewel, Clippard, Vent/arm sounds and firing together.
    // Restore VENT_SWITCH_PIN = 1 and VENT_FOLLOWS_WAND_SWITCH = false to re-enable.
    constexpr int8_t VENT_SWITCH_PIN = -1;
    constexpr bool BENCH_ENABLE_WITHOUT_VENT_SWITCH = false;
    constexpr bool VENT_FOLLOWS_WAND_SWITCH = true;  // only when VENT_SWITCH_PIN is -1
    constexpr int8_t WAND_SWITCH_PIN = 20;
    constexpr bool BENCH_ENABLE_WITHOUT_WAND_SWITCH = false;
    constexpr int8_t FIRE_BUTTON_PIN = 2;
    constexpr int8_t PIXEL_PIN = 21;

    constexpr bool PACK_LINK_ENABLED = true;
    constexpr int8_t PACK_RX_PIN = 6;
    constexpr int8_t PACK_TX_PIN = 7;

    constexpr int8_t BARGRAPH_SDA_PIN = 4;
    constexpr int8_t BARGRAPH_SCL_PIN = 5;
    constexpr uint8_t BARGRAPH_I2C_ADDRESS = 0x70;
    constexpr uint8_t BARGRAPH_SEGMENTS = 28;
    constexpr uint8_t BARGRAPH_BRIGHTNESS = 1;
    constexpr uint32_t BARGRAPH_DIAGNOSTIC_STEP_MS = 250;
    constexpr uint32_t BARGRAPH_DIAGNOSTIC_BLANK_MS = 1000;
    constexpr uint32_t BARGRAPH_FIRING_STEP_MS = 15;  // GPStar level 5; faster near overheat
    constexpr uint32_t BARGRAPH_COOLDOWN_STEP_MS = 80;

    constexpr uint16_t SLO_BLO_PIXEL = 0;
    constexpr uint16_t VENT_FIRST_PIXEL = SLO_BLO_PIXEL + 1;
    constexpr uint16_t VENT_PIXELS = 7;
    constexpr uint16_t HAT_PIXEL = VENT_FIRST_PIXEL + VENT_PIXELS;
    constexpr uint16_t SECOND_HAT_PIXEL = HAT_PIXEL + 1;
    constexpr uint16_t CLIPPARD_PIXEL = SECOND_HAT_PIXEL + 1;
    constexpr uint16_t FIRING_FIRST_PIXEL = CLIPPARD_PIXEL + 1;
    constexpr uint16_t FIRING_PIXELS = 23;
    constexpr uint16_t FIRING_TIP_PIXEL = FIRING_FIRST_PIXEL + 11;
    constexpr uint16_t PIXEL_COUNT = FIRING_FIRST_PIXEL + FIRING_PIXELS;
    constexpr uint8_t PIXEL_BRIGHTNESS = 255;
    constexpr uint8_t DIAGNOSTIC_BRIGHTNESS = 16;
    constexpr uint32_t DIAGNOSTIC_PIXEL_MS = 200;
    constexpr uint32_t DIAGNOSTIC_COLOR_MS = 1000;

    constexpr uint32_t SLO_BLO_COLOR = 0xFF3000;
    constexpr uint32_t VENT_COLOR = 0xFF5000;
    constexpr uint32_t HAT_COLOR = 0xFFFFFF;
    constexpr uint32_t SECOND_HAT_COLOR = 0xFFFFFF;
    constexpr uint32_t CLIPPARD_COLOR = 0xFFFFFF;
    constexpr uint32_t STREAM_COLOR = 0xA00000;
    constexpr uint32_t STREAM_HEAD_COLOR = 0x2080FF;
    constexpr uint32_t STREAM_TIP_COLOR = 0xFFFFFF;

    constexpr uint16_t DEBOUNCE_MS = 25;
    constexpr uint32_t STARTUP_MS = 1500;
    constexpr uint32_t MAX_FIRE_MS = 8000;
    // GPStar overheat: vent opens, steam after 4 s, vent closes after 6 s more
    // (i_overheating_delay + level 5 i_ms_overheating_length). Must match pack.
    constexpr uint32_t COOLDOWN_MS = 10000;
    // Holding fire this long with Activate off toggles pack sound mute (saved in flash).
    constexpr uint32_t MUTE_HOLD_MS = 3000;
    constexpr uint32_t FRAME_INTERVAL_MS = 20;
    constexpr uint32_t FIRING_FRAME_INTERVAL_MS = 4;
    constexpr uint32_t FIRING_STEP_MS = 8;
    constexpr uint16_t FIRING_BURST_MIN_PIXELS = 2;
    constexpr uint16_t FIRING_BURST_MAX_PIXELS = 6;
    constexpr uint32_t FIRING_STROBE_ON_MS = 32;
    constexpr uint32_t FIRING_STROBE_PERIOD_MS = 64;
    constexpr uint32_t FIRING_TIP_FLASH_MS = 16;
    constexpr uint32_t FIRING_TIP_PERIOD_MS = 64;

    constexpr bool validPin(int8_t pin)
    {
        return pin == -1 || (pin >= 0 && pin <= 10) || pin == 20 || pin == 21;
    }

    constexpr bool separatePins(int8_t first, int8_t second)
    {
        return first < 0 || second < 0 || first != second;
    }

    constexpr bool availableLinkPin(int8_t pin)
    {
        return pin >= 0 && validPin(pin) && pin != ACTIVATE_SWITCH_PIN &&
               pin != VENT_SWITCH_PIN && pin != WAND_SWITCH_PIN && pin != FIRE_BUTTON_PIN &&
               pin != PIXEL_PIN && pin != BARGRAPH_SDA_PIN && pin != BARGRAPH_SCL_PIN;
    }

    static_assert(!PACK_LINK_ENABLED || (availableLinkPin(PACK_RX_PIN) &&
                      availableLinkPin(PACK_TX_PIN) && PACK_RX_PIN != PACK_TX_PIN),
                  "Pack UART requires two unused exposed C3 GPIOs");
    static_assert(validPin(ACTIVATE_SWITCH_PIN) && validPin(VENT_SWITCH_PIN) &&
                      validPin(WAND_SWITCH_PIN) && validPin(FIRE_BUTTON_PIN) && validPin(PIXEL_PIN),
                  "Use exposed C3 Super Mini GPIOs, or -1 to disable a connection");
    static_assert(separatePins(ACTIVATE_SWITCH_PIN, VENT_SWITCH_PIN) &&
                      separatePins(ACTIVATE_SWITCH_PIN, WAND_SWITCH_PIN) &&
                      separatePins(ACTIVATE_SWITCH_PIN, FIRE_BUTTON_PIN) &&
                      separatePins(ACTIVATE_SWITCH_PIN, PIXEL_PIN) &&
                      separatePins(VENT_SWITCH_PIN, WAND_SWITCH_PIN) &&
                      separatePins(VENT_SWITCH_PIN, FIRE_BUTTON_PIN) &&
                      separatePins(VENT_SWITCH_PIN, PIXEL_PIN) &&
                      separatePins(WAND_SWITCH_PIN, FIRE_BUTTON_PIN) &&
                      separatePins(WAND_SWITCH_PIN, PIXEL_PIN) &&
                      separatePins(FIRE_BUTTON_PIN, PIXEL_PIN),
                  "Wand inputs and LEDs must use distinct GPIOs");
    static_assert(BARGRAPH_SDA_PIN >= 0 && BARGRAPH_SCL_PIN >= 0 &&
                      validPin(BARGRAPH_SDA_PIN) && validPin(BARGRAPH_SCL_PIN) &&
                      separatePins(BARGRAPH_SDA_PIN, BARGRAPH_SCL_PIN) &&
                      separatePins(BARGRAPH_SDA_PIN, ACTIVATE_SWITCH_PIN) &&
                      separatePins(BARGRAPH_SDA_PIN, VENT_SWITCH_PIN) &&
                      separatePins(BARGRAPH_SDA_PIN, WAND_SWITCH_PIN) &&
                      separatePins(BARGRAPH_SDA_PIN, FIRE_BUTTON_PIN) &&
                      separatePins(BARGRAPH_SDA_PIN, PIXEL_PIN) &&
                      separatePins(BARGRAPH_SCL_PIN, ACTIVATE_SWITCH_PIN) &&
                      separatePins(BARGRAPH_SCL_PIN, VENT_SWITCH_PIN) &&
                      separatePins(BARGRAPH_SCL_PIN, WAND_SWITCH_PIN) &&
                      separatePins(BARGRAPH_SCL_PIN, FIRE_BUTTON_PIN) &&
                      separatePins(BARGRAPH_SCL_PIN, PIXEL_PIN),
                  "Bargraph I2C requires two unused exposed GPIOs");
    static_assert(BARGRAPH_I2C_ADDRESS >= 0x70 && BARGRAPH_I2C_ADDRESS <= 0x77 &&
                      BARGRAPH_SEGMENTS == 28 && BARGRAPH_BRIGHTNESS <= 15 &&
                      BARGRAPH_DIAGNOSTIC_STEP_MS > 0 && BARGRAPH_DIAGNOSTIC_BLANK_MS > 0 &&
                      BARGRAPH_FIRING_STEP_MS > 0 && BARGRAPH_COOLDOWN_STEP_MS > 0,
                  "Bargraph requires an HT16K33 address, 28 segments, brightness 0-15, and positive timings");
    static_assert(VENT_PIXELS > 0 && FIRING_PIXELS > 0 && PIXEL_COUNT > FIRING_FIRST_PIXEL,
                  "Wand LED sections must contain pixels and fit within the chain");
    static_assert(FIRING_TIP_PIXEL >= FIRING_FIRST_PIXEL && FIRING_TIP_PIXEL < PIXEL_COUNT,
                  "Tip strobe must stay within the firing strip");
    static_assert(DEBOUNCE_MS > 0 && STARTUP_MS > 0 && MAX_FIRE_MS > 0 && COOLDOWN_MS > 0 &&
                      FRAME_INTERVAL_MS > 0 && FIRING_FRAME_INTERVAL_MS > 0 &&
                      FIRING_STEP_MS >= FIRING_FRAME_INTERVAL_MS,
                  "Wand timing intervals must be positive");
    static_assert(BARGRAPH_SEGMENTS * BARGRAPH_COOLDOWN_STEP_MS <= COOLDOWN_MS,
                  "Bargraph cooldown ramp must complete within the firing lockout");
    static_assert(FIRING_BURST_MIN_PIXELS > 0 && FIRING_BURST_MIN_PIXELS <= FIRING_BURST_MAX_PIXELS &&
                      FIRING_BURST_MAX_PIXELS < FIRING_PIXELS,
                  "Firing bursts must fit the strip");
    static_assert(FIRING_STROBE_ON_MS >= FIRING_FRAME_INTERVAL_MS &&
                      FIRING_STROBE_PERIOD_MS >= FIRING_STROBE_ON_MS + FIRING_FRAME_INTERVAL_MS,
                  "Firing strobe must have visible on and off intervals");
    static_assert(FIRING_TIP_FLASH_MS >= FIRING_FRAME_INTERVAL_MS &&
                      FIRING_TIP_PERIOD_MS >= FIRING_TIP_FLASH_MS + FIRING_FRAME_INTERVAL_MS,
                  "Tip strobe must have visible on and off intervals");
    static_assert(DIAGNOSTIC_BRIGHTNESS > 0 && DIAGNOSTIC_PIXEL_MS > 0 && DIAGNOSTIC_COLOR_MS > 0,
                  "LED diagnostic brightness and intervals must be positive");
}