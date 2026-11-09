#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "WandController.h"

class WandLights
{
public:
    void begin()
    {
        if (wand_config::PIXEL_PIN < 0)
            return;
        if (pixels.numPixels() != wand_config::PIXEL_COUNT)
        {
            Serial.println("NeoPixel allocation failed; LED output disabled");
            return;
        }

        pixels.begin();
        pixels.setBrightness(wand_config::LED_DIAGNOSTIC_MODE ? wand_config::DIAGNOSTIC_BRIGHTNESS : wand_config::PIXEL_BRIGHTNESS);
        pixels.clear();
        pixels.show();
        diagnosticStarted = millis();
        enabled = true;
    }

    void updateDiagnostic(uint32_t now)
    {
        if (!enabled || now - lastFrame < wand_config::FRAME_INTERVAL_MS)
            return;

        lastFrame = now;
        static constexpr uint32_t colors[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF, 0x000000};
        constexpr uint32_t colorCount = sizeof(colors) / sizeof(colors[0]);
        constexpr uint32_t chaseDuration = wand_config::PIXEL_COUNT * wand_config::DIAGNOSTIC_PIXEL_MS;
        constexpr uint32_t cycleDuration = chaseDuration + colorCount * wand_config::DIAGNOSTIC_COLOR_MS;
        const uint32_t position = (now - diagnosticStarted) % cycleDuration;

        pixels.clear();
        if (position < chaseDuration)
            pixels.setPixelColor(position / wand_config::DIAGNOSTIC_PIXEL_MS, 0xFFFFFF);
        else
            pixels.fill(colors[(position - chaseDuration) / wand_config::DIAGNOSTIC_COLOR_MS]);
        pixels.show();
    }

    void update(const WandController &wand, const WandControls &controls, uint32_t now)
    {
        const uint32_t frameInterval = wand.state() == WandState::Firing ?
                                       wand_config::FIRING_FRAME_INTERVAL_MS : wand_config::FRAME_INTERVAL_MS;
        if (!enabled || (wand.state() == lastState &&
                         now - lastFrame < frameInterval))
            return;

        lastFrame = now;
        lastState = wand.state();
        pixels.clear();

        uint8_t brightness = wand_config::PIXEL_BRIGHTNESS;
        if (wand.state() == WandState::Starting && wand.elapsed(now) < wand_config::STARTUP_MS)
            brightness = wand_config::PIXEL_BRIGHTNESS * wand.elapsed(now) / wand_config::STARTUP_MS;
        pixels.setBrightness(brightness);

        if (wand.state() != WandState::Off)
        {
            const bool cooling = wand.state() == WandState::Cooling;
            const bool blinkOn = (wand.elapsed(now) / 250) % 2 == 0;
            if (!cooling || blinkOn)
            {
                pixels.setPixelColor(wand_config::SLO_BLO_PIXEL, wand_config::SLO_BLO_COLOR);
                pixels.setPixelColor(wand_config::HAT_PIXEL, wand_config::HAT_COLOR);
                if (controls.vent)
                    pixels.fill(wand_config::VENT_COLOR, wand_config::VENT_FIRST_PIXEL, wand_config::VENT_PIXELS);
            }
            if (!cooling || !blinkOn)
                pixels.setPixelColor(wand_config::SECOND_HAT_PIXEL, wand_config::SECOND_HAT_COLOR);
            if (controls.wand)
                pixels.setPixelColor(wand_config::CLIPPARD_PIXEL, wand_config::CLIPPARD_COLOR);

            if (wand.state() == WandState::Firing)
            {
                const uint32_t elapsed = wand.elapsed(now);
                const uint32_t step = elapsed / wand_config::FIRING_STEP_MS;
                const uint16_t head = step % wand_config::FIRING_PIXELS;
                const uint16_t filled = step < wand_config::FIRING_PIXELS ? head + 1 : wand_config::FIRING_PIXELS;
                const bool strobeOn = elapsed % wand_config::FIRING_STROBE_PERIOD_MS < wand_config::FIRING_STROBE_ON_MS;
                const bool alternate = (elapsed / wand_config::FIRING_STROBE_PERIOD_MS) % 2 != 0;
                if (strobeOn)
                {
                    pixels.fill(alternate ? wand_config::STREAM_HEAD_COLOR : wand_config::STREAM_COLOR,
                                wand_config::FIRING_FIRST_PIXEL, filled);
                    const uint32_t burstColor = alternate ? wand_config::STREAM_COLOR : wand_config::STREAM_HEAD_COLOR;
                    const uint16_t burstPixels = static_cast<uint16_t>(random(
                        wand_config::FIRING_BURST_MIN_PIXELS, wand_config::FIRING_BURST_MAX_PIXELS + 1));
                    for (uint16_t offset = 0; offset < burstPixels; ++offset)
                    {
                        const uint16_t relative = (head + wand_config::FIRING_PIXELS - offset) %
                                                  wand_config::FIRING_PIXELS;
                        if (relative < filled)
                            pixels.setPixelColor(wand_config::FIRING_FIRST_PIXEL + relative, burstColor);
                    }
                }

                const bool tipOn = elapsed % wand_config::FIRING_TIP_PERIOD_MS < wand_config::FIRING_TIP_FLASH_MS;
                pixels.setPixelColor(wand_config::FIRING_TIP_PIXEL, tipOn ? wand_config::STREAM_TIP_COLOR : 0);
            }
        }

        pixels.show();
    }

private:
    Adafruit_NeoPixel pixels{wand_config::PIXEL_COUNT, wand_config::PIXEL_PIN, NEO_GRB + NEO_KHZ800};
    bool enabled = false;
    uint32_t lastFrame = 0;
    uint32_t diagnosticStarted = 0;
    WandState lastState = WandState::Off;
};