#pragma once

#include <Adafruit_NeoPixel.h>
#include "PackController.h"
#include "Cyclotron.h"

class PackLights
{
public:
    void begin(const PackController &pack, uint32_t now)
    {
        ring.begin();
        ring.setBrightness(config::RING_BRIGHTNESS);
        if (config::POWER_BAR_PIN >= 0)
        {
            powerBar.begin();
            powerBar.setBrightness(config::POWER_BAR_BRIGHTNESS);
        }
        lastFrameMs = now;
        powerBarStepStarted = now;
        powerBarState = pack.state();
        lastTheme = pack.theme();
        lastState = pack.state();
        cyclotron.begin(now);
        afterlife.begin(now);
        render(pack, now);
    }

    // Turn every pack pixel off (OTA updates pause the loop).
    void blank()
    {
        ring.clear();
        ring.show();
        if (config::POWER_BAR_PIN >= 0)
        {
            powerBar.clear();
            powerBar.show();
        }
    }

    void update(const PackController &pack, uint32_t now, uint8_t speedLevel = 1)
    {
        const uint32_t elapsed = now - lastFrameMs;
        if (elapsed < config::FRAME_INTERVAL_MS)
            return;

        lastFrameMs = now;
        currentSpeedLevel = pack.state() == PackState::Firing ? speedLevel : 1;
        const bool immediateSpeed = config::WAND_LINK_ENABLED &&
                                    pack.state() != PackState::Stopping &&
                                    (pack.state() == PackState::Firing ||
                                     lastState == PackState::Firing ||
                                     pack.state() == PackState::Cooling);
        // A theme change restarts the new style with its startup spin-up.
        if (pack.theme() != lastTheme ||
            (pack.state() == PackState::Starting && lastState != PackState::Starting))
        {
            cyclotron.begin(now);
            afterlife.begin(now);
        }
        lastTheme = pack.theme();
        lastState = pack.state();
        const uint32_t shutdownRampMs = config::themeShutdownRampMs(lastTheme);

        if (pack.cyclotronMode() == CyclotronMode::Afterlife)
        {
            uint32_t targetStepMs = config::AFTERLIFE_IDLE_STEP_MS;
            if (pack.state() == PackState::Firing)
            {
                if (config::WAND_LINK_ENABLED)
                {
                    // Scale GPStar's 40-LED intervals (7, 5, 4, 3 ms) to our window chase.
                    const uint8_t relativeStep = currentSpeedLevel > 1 ? 7 - currentSpeedLevel : 7;
                    targetStepMs = (config::AFTERLIFE_IDLE_STEP_MS * relativeStep + 3) / 7;
                }
                else
                    targetStepMs = config::AFTERLIFE_FIRING_STEP_MS;
            }
            else if (pack.state() == PackState::Stopping)
                targetStepMs = config::AFTERLIFE_START_STEP_MS;

            if (pack.state() != PackState::Off)
                afterlife.update(now, targetStepMs, immediateSpeed,
                                 pack.state() == PackState::Stopping ?
                                 shutdownRampMs : config::AFTERLIFE_RAMP_MS);
        }
        else
        {
            // GPStar cyclotron1984(): the step is divided by the firing multiplier.
            uint32_t stepMs = config::CYCLOTRON_1984_STEP_MS;
            uint32_t rampMs = config::CYCLOTRON_1984_RAMP_MS;
            if (pack.state() == PackState::Firing)
                stepMs = config::WAND_LINK_ENABLED ? stepMs / currentSpeedLevel : stepMs / 2;
            else if (pack.state() == PackState::Stopping)
            {
                stepMs = config::CYCLOTRON_1984_START_STEP_MS;
                rampMs = shutdownRampMs;
            }
            if (pack.state() != PackState::Off)
                cyclotron.update(now, stepMs, immediateSpeed, rampMs);
        }

        render(pack, now);
    }

private:
    Adafruit_NeoPixel ring{config::RING_PIXELS, config::RING_PIN, NEO_GRB + NEO_KHZ800};
    Adafruit_NeoPixel powerBar{
        config::POWER_BAR_PIN >= 0 ? config::POWER_BAR_PIXELS : 0,
        config::POWER_BAR_PIN, NEO_GRB + NEO_KHZ800};
    uint32_t lastFrameMs = 0;
    Cyclotron1984 cyclotron;
    AfterlifeCyclotron afterlife;
    SystemTheme lastTheme = config::SYSTEM_THEME;
    PackState lastState = PackState::Off;
    uint8_t currentSpeedLevel = 1;
    uint16_t powerBarLit = 0;
    uint32_t powerBarStepStarted = 0;
    PackState powerBarState = PackState::Off;

    float brightnessScale(const PackController &pack, uint32_t now) const
    {
        switch (pack.state())
        {
        case PackState::Off:
            return 0.0f;
        case PackState::Starting:
            return min(1.0f, static_cast<float>(pack.stateAge(now)) / config::STARTUP_MS);
        case PackState::Stopping:
        {
            // Full brightness while the cyclotron slows, then the modern themes fade
            // out (GPStar fadeOutCyclotron); 1984/1989 switch off at the end of the ramp.
            const uint32_t age = pack.stateAge(now);
            const uint32_t rampMs = config::themeShutdownRampMs(pack.theme());
            const uint32_t fadeMs = config::themeShutdownFadeMs(pack.theme());
            if (age < rampMs)
                return 1.0f;
            if (fadeMs == 0)
                return 0.0f;
            return 1.0f - min(1.0f, static_cast<float>(age - rampMs) / fadeMs);
        }
        case PackState::Cooling:
            return 0.25f;
        default:
            return 1.0f;
        }
    }

    void render(const PackController &pack, uint32_t now)
    {
        const float scale = brightnessScale(pack, now);
        for (uint16_t pixel = 0; pixel < config::RING_PIXELS; ++pixel)
        {
            const bool isAfterlife = pack.cyclotronMode() == CyclotronMode::Afterlife;
            const uint8_t redLevel = isAfterlife ? afterlife.redAt(pixel) : cyclotron.redAt(pixel);
            const uint8_t greenLevel = isAfterlife ? afterlife.greenAt(pixel) : cyclotron.greenAt(pixel);
            const uint8_t red = static_cast<uint8_t>(redLevel * scale + 0.5f);
            const uint8_t green = static_cast<uint8_t>(greenLevel * scale + 0.5f);
            ring.setPixelColor(pixel, ring.Color(red, green, 0));
        }
        ring.show();

        if (config::POWER_BAR_PIN >= 0)
            renderPowerBar(pack, now, scale);
    }

    void renderPowerBar(const PackController &pack, uint32_t now, float scale)
    {
        uint32_t stepMs = config::POWER_BAR_STEP_MS;
        if (pack.state() == PackState::Firing)
        {
            if (config::WAND_LINK_ENABLED)
            {
                const uint32_t reduction = 5 * (currentSpeedLevel - 1);
                stepMs = stepMs > reduction ? stepMs - reduction : 1;
            }
            else
                stepMs = max(uint32_t{1}, stepMs / 2);
        }

        // GPStar powercellLoop(): while the Afterlife cyclotron ramps, each power-cell
        // step also waits the current cyclotron step, so it speeds up and slows down with it.
        const bool afterlifeRamp = pack.cyclotronMode() == CyclotronMode::Afterlife &&
                                   pack.state() != PackState::Firing &&
                                   afterlife.stepIntervalMs() > config::AFTERLIFE_IDLE_STEP_MS;
        if (afterlifeRamp)
            stepMs += afterlife.stepIntervalMs();

        if (powerBarState != pack.state())
        {
            // Keep cycling through Afterlife startup/shutdown instead of restarting.
            const bool continues = pack.cyclotronMode() == CyclotronMode::Afterlife &&
                                   ((powerBarState == PackState::Starting && pack.state() == PackState::Idle) ||
                                    pack.state() == PackState::Stopping);
            if (!continues)
                powerBarLit = 0;
            powerBarStepStarted = now;
            powerBarState = pack.state();
        }
        const uint32_t steps = (now - powerBarStepStarted) / stepMs;
        powerBarStepStarted += steps * stepMs;
        powerBarLit = (powerBarLit + steps % (config::POWER_BAR_PIXELS + 1)) %
                      (config::POWER_BAR_PIXELS + 1);
        uint16_t litPixels = powerBarLit;
        if ((pack.state() == PackState::Starting || pack.state() == PackState::Stopping) &&
            pack.cyclotronMode() != CyclotronMode::Afterlife)
            litPixels = static_cast<uint16_t>(config::POWER_BAR_PIXELS * scale);
        else if (pack.state() == PackState::Cooling)
            litPixels = config::POWER_BAR_PIXELS;
        else if (pack.state() == PackState::Off)
            litPixels = 0;

        powerBar.clear();
        const uint8_t blue = static_cast<uint8_t>(255.0f * scale + 0.5f);
        for (uint16_t pixel = 0; pixel < litPixels; ++pixel)
            powerBar.setPixelColor(pixel, powerBar.Color(0, 0, blue));
        powerBar.show();
    }
};
