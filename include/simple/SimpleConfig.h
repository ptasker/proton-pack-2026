#pragma once

#include <stdint.h>

// Simple pack (second build): one ESP32-WROOM drives the pack and wand lights,
// reads the wand's Activate switch and fire button, and plays the same SD sounds
// as the main pack. Timing, sounds and themes come from the main pack's Config.h
// and WandConfig.h (included unchanged); everything here is simple-pack only.
namespace simple_config
{
    // Inputs: switch/button to GND, internal pull-ups (verified 2026-10-17).
    constexpr int8_t ACTIVATE_PIN = 33;
    constexpr int8_t FIRE_PIN = 32;
    constexpr uint16_t DEBOUNCE_MS = 25;

    // Cyclotron: 4 x 7-pixel jewels chained on GPIO22. Each jewel is one lens:
    // its 6 outer pixels follow the main pack's 6-pixel lens window and the centre
    // pixel follows the brightest of them.
    constexpr int8_t CYCLOTRON_PIN = 22;
    constexpr uint16_t JEWELS = 4;
    constexpr uint16_t JEWEL_PIXELS = 7;
    constexpr uint16_t CYCLOTRON_PIXELS = JEWELS * JEWEL_PIXELS;
    constexpr bool JEWEL_CENTER_FIRST = true;          // Adafruit-style jewel: pixel 0 is the centre
    constexpr uint8_t CYCLOTRON_BRIGHTNESS = 255;
    // Multiplies the main pack's dim warm-red cyclotron levels (peak ~51/255).
    constexpr uint16_t CYCLOTRON_GAIN_PERCENT = 300;

    // Power cell: 12-pixel strip on GPIO23 (the main pack's animation, 12 pixels).
    constexpr int8_t POWER_CELL_PIN = 23;
    constexpr uint16_t POWER_CELL_PIXELS = 12;
    constexpr uint8_t POWER_CELL_BRIGHTNESS = 32;

    // Wand: one chain of 10 on GPIO21, in data order.
    constexpr int8_t WAND_PIN = 21;
    constexpr uint16_t BARGRAPH_FIRST = 0;             // 4-pixel mini bargraph
    constexpr uint16_t BARGRAPH_PIXELS = 4;
    constexpr bool BARGRAPH_FIRST_IS_BOTTOM = true;
    constexpr uint16_t BODY_1_PIXEL = 4;               // slo-blo
    constexpr uint16_t BODY_2_PIXEL = 5;               // Clippard/hat white
    constexpr uint16_t VENT_PIXEL = 6;
    constexpr uint16_t FIRING_FIRST = 7;               // 3-pixel firing strip; the last is the tip
    constexpr uint16_t FIRING_PIXELS = 3;
    constexpr uint16_t WAND_PIXELS = FIRING_FIRST + FIRING_PIXELS;
    constexpr uint8_t WAND_BRIGHTNESS = 255;

    constexpr uint32_t BARGRAPH_COLOR = 0xFF8000;
    constexpr uint8_t BARGRAPH_LEVEL_PERCENT = 25;      // dimmer bargraph (by request, 2026-10-20)

    // Master volume until one is saved from the web page or USB +/-. Lower peaks
    // reduce the battery dips that caused brownout resets while firing.
    constexpr int8_t DEFAULT_VOLUME_DB = -6;
    constexpr uint32_t BODY_1_COLOR = 0x0000FF;        // blue (blue lens cover, by request)
    constexpr uint32_t BODY_2_COLOR = 0xFFFFFF;        // main wand hats/Clippard
    constexpr uint32_t VENT_COLOR = 0xFF5000;          // main wand vent
    constexpr uint32_t STREAM_COLOR = 0xA00000;        // main wand firing colours
    constexpr uint32_t STREAM_ALT_COLOR = 0x2080FF;
    constexpr uint32_t STREAM_TIP_COLOR = 0xFFFFFF;

    // No Vent or arm switches: both follow Activate. With ARM_BEEPS the pack plays
    // GPStar's armed-wand beep loop while ready (false = no beeping).
    constexpr bool ARM_BEEPS = false;  // disabled by request (2026-10-17)
    // Afterlife/Frozen Empire wand "ready" whine (147 fade-in, 144 loop) plays while
    // Vent is on, and Vent always follows Activate here. false (by request,
    // 2026-10-20) keeps it silent; wand shutdown sound and pack hum are unchanged.
    constexpr bool WAND_READY_HUM = false;

    static_assert(BARGRAPH_LEVEL_PERCENT <= 100 && DEFAULT_VOLUME_DB <= 0 && DEFAULT_VOLUME_DB >= -40,
                  "Bargraph level 0-100 %, volume -40..0 dB");
    static_assert(CYCLOTRON_PIXELS == 28 && JEWEL_PIXELS == 7, "Cyclotron jewel mapping expects 7-pixel jewels");
    static_assert(BARGRAPH_FIRST + BARGRAPH_PIXELS <= BODY_1_PIXEL && BODY_1_PIXEL < BODY_2_PIXEL &&
                      BODY_2_PIXEL < VENT_PIXEL && VENT_PIXEL < FIRING_FIRST && FIRING_PIXELS > 0,
                  "Wand chain layout must be in data order without overlaps");
}
