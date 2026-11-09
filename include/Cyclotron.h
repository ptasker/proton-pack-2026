#pragma once

#include <math.h>
#include "Config.h"

class Cyclotron
{
public:
    void begin(uint32_t now)
    {
        lastStepMs = now;
        headPixel = 0;
        trailAge = 0;
    }

    void update(uint32_t now, uint32_t stepMs)
    {
        if (stepMs == 0)
            return;

        const uint32_t steps = (now - lastStepMs) / stepMs;
        if (steps == 0)
            return;

        lastStepMs += steps * stepMs;
        headPixel = (headPixel + steps % config::RING_PIXELS) % config::RING_PIXELS;
        if (steps >= 8 || trailAge + steps >= 8)
            trailAge = 8;
        else
            trailAge += steps;
    }

    uint8_t redAt(uint16_t pixel) const
    {
        if (pixel >= config::RING_PIXELS)
            return 0;

        const uint16_t distance = (headPixel + config::RING_PIXELS - pixel) % config::RING_PIXELS;
        if (distance > trailAge || distance >= 8)
            return 0;

        return static_cast<uint8_t>(255U >> distance);
    }

private:
    uint32_t lastStepMs = 0;
    uint16_t headPixel = 0;
    uint8_t trailAge = 0;
};

class AfterlifeCyclotron
{
public:
    void begin(uint32_t now)
    {
        lastStepMs = now;
        rampStartedMs = now;
        headSlot = 0;
        currentStepMs = config::AFTERLIFE_START_STEP_MS;
        rampFromMs = currentStepMs;
        rampTargetMs = config::AFTERLIFE_IDLE_STEP_MS;
        rampDurationMs = config::AFTERLIFE_RAMP_MS;
        frameTimeMs = now;
        for (Pulse &pulse : pulses)
            pulse = {};
        startPulse(headSlot, now);
    }

    void update(uint32_t now, uint32_t targetStepMs, bool immediate = false,
                uint32_t rampMs = config::AFTERLIFE_RAMP_MS)
    {
        if (targetStepMs == 0 || targetStepMs > config::AFTERLIFE_START_STEP_MS)
            return;

        if (immediate)
        {
            currentStepMs = targetStepMs;
            rampFromMs = targetStepMs;
            rampTargetMs = targetStepMs;
            rampStartedMs = now;
            rampDurationMs = rampMs;
        }
        else
        {
            updateInterval(now);
            if (targetStepMs != rampTargetMs || rampMs != rampDurationMs)
            {
                rampFromMs = currentStepMs;
                rampTargetMs = targetStepMs;
                rampStartedMs = now;
                rampDurationMs = rampMs;
            }
        }

        const uint32_t steps = (now - lastStepMs) / currentStepMs;
        lastStepMs += steps * currentStepMs;
        // Only the latest lap can still contribute light after a delayed update.
        const uint16_t visibleSteps = steps < VISIBLE_PIXELS ?
                                      static_cast<uint16_t>(steps) : VISIBLE_PIXELS;
        headSlot = (headSlot + (steps - visibleSteps) % VISIBLE_PIXELS) % VISIBLE_PIXELS;
        for (uint16_t step = 0; step < visibleSteps; ++step)
        {
            headSlot = (headSlot + 1) % VISIBLE_PIXELS;
            startPulse(headSlot, lastStepMs - (visibleSteps - 1 - step) * currentStepMs);
        }
        frameTimeMs = now;
        for (Pulse &pulse : pulses)
        {
            if (pulse.active && now - pulse.startedMs >= pulse.riseMs + pulse.fallMs)
                pulse.active = false;
        }
    }

    uint32_t stepIntervalMs() const
    {
        return currentStepMs;
    }

    uint8_t redAt(uint16_t pixel) const
    {
        if (pixel >= config::RING_PIXELS)
            return 0;

        for (uint16_t window = 0; window < WINDOW_COUNT; ++window)
        {
            const uint16_t start = (config::AFTERLIFE_WINDOW_OFFSET +
                                    window * config::RING_PIXELS / WINDOW_COUNT +
                                    config::RING_PIXELS - config::AFTERLIFE_WINDOW_PIXELS / 2) %
                                   config::RING_PIXELS;
            const uint16_t position = (pixel + config::RING_PIXELS - start) % config::RING_PIXELS;
            if (position >= config::AFTERLIFE_WINDOW_PIXELS)
                continue;

            const uint16_t slot = window * config::AFTERLIFE_WINDOW_PIXELS + position;
            return pulseLevel(pulses[slot]);
        }
        return 0;
    }

    uint8_t greenAt(uint16_t pixel) const
    {
        return redAt(pixel) / 25;
    }

private:
    static constexpr uint16_t WINDOW_COUNT = 4;
    static constexpr uint16_t VISIBLE_PIXELS = WINDOW_COUNT * config::AFTERLIFE_WINDOW_PIXELS;
    struct Pulse
    {
        uint32_t startedMs;
        uint32_t riseMs;
        uint32_t fallMs;
        uint8_t peak;
        bool active;
    };
    Pulse pulses[VISIBLE_PIXELS] = {};
    uint32_t frameTimeMs = 0;
    uint32_t lastStepMs = 0;
    uint32_t rampStartedMs = 0;
    uint32_t currentStepMs = config::AFTERLIFE_START_STEP_MS;
    uint32_t rampFromMs = config::AFTERLIFE_START_STEP_MS;
    uint32_t rampTargetMs = config::AFTERLIFE_IDLE_STEP_MS;
    uint32_t rampDurationMs = config::AFTERLIFE_RAMP_MS;
    uint16_t headSlot = 0;

    void startPulse(uint16_t slot, uint32_t now)
    {
        const bool ramping = currentStepMs != rampTargetMs;
        const bool accelerated = currentStepMs < config::AFTERLIFE_IDLE_STEP_MS;
        const uint32_t riseMs = currentStepMs * (ramping || accelerated ? 1 : 3);
        const uint32_t factor = 1000 * (config::AFTERLIFE_START_STEP_MS - currentStepMs) /
                                (config::AFTERLIFE_START_STEP_MS - 1);
        const uint32_t brightness = 100 + 255 * factor / 1000;
        const uint8_t peak = static_cast<uint8_t>((brightness > 255 ? 255 : brightness) / 5);
        pulses[slot] = {now, riseMs, currentStepMs * 3, peak, true};
    }

    uint8_t pulseLevel(const Pulse &pulse) const
    {
        if (!pulse.active)
            return 0;
        const uint32_t age = frameTimeMs - pulse.startedMs;
        if (age >= pulse.riseMs + pulse.fallMs)
            return 0;

        float level;
        if (age < pulse.riseMs)
        {
            const float progress = static_cast<float>(age) / pulse.riseMs;
            level = 1.0f - sqrtf(1.0f - progress * progress);
        }
        else
        {
            const float remaining = 1.0f - static_cast<float>(age - pulse.riseMs) / pulse.fallMs;
            level = 1.0f - sqrtf(1.0f - remaining * remaining);
        }
        return static_cast<uint8_t>(pulse.peak * level + 0.5f);
    }

    void updateInterval(uint32_t now)
    {
        const uint32_t elapsed = now - rampStartedMs;
        if (elapsed >= rampDurationMs)
        {
            currentStepMs = rampTargetMs;
            return;
        }

        const float progress = static_cast<float>(elapsed) / rampDurationMs;
        const bool accelerating = rampTargetMs < rampFromMs;
        const float curve = accelerating ? 1.0f - progress : progress;
        const float squared = curve * curve;
        const float eased = accelerating ? 1.0f - squared * squared : squared * squared;
        const float difference = static_cast<float>(rampTargetMs) - rampFromMs;
        currentStepMs = static_cast<uint32_t>(rampFromMs + difference * eased + 0.5f);
    }
};

// GPStar cyclotron1984() for the 1984 and 1989 themes: the four lens windows
// light one at a time in turn. The newly lit window turns on fully; the previous
// one fades out linearly over 1.5 steps (cyclotron84LightOff with LED fades on).
// Speed ramps use circular easing like GPStar's 1984 ramps. Brightness matches
// the dim warm red peak of the Afterlife renderer.
class Cyclotron1984
{
public:
    void begin(uint32_t now)
    {
        position = 0;
        lastStepMs = now;
        currentStepMs = config::CYCLOTRON_1984_START_STEP_MS;
        rampFromMs = currentStepMs;
        rampTargetMs = config::CYCLOTRON_1984_STEP_MS;
        rampStartedMs = now;
        rampDurationMs = config::CYCLOTRON_1984_RAMP_MS;
        frameTimeMs = now;
        for (Fade &fade : fades)
            fade = {};
    }

    void update(uint32_t now, uint32_t targetStepMs, bool immediate, uint32_t rampMs)
    {
        if (targetStepMs == 0)
            return;
        if (immediate)
        {
            currentStepMs = rampFromMs = rampTargetMs = targetStepMs;
            rampStartedMs = now;
            rampDurationMs = rampMs;
        }
        else
        {
            updateInterval(now);
            if (targetStepMs != rampTargetMs || rampMs != rampDurationMs)
            {
                rampFromMs = currentStepMs;
                rampTargetMs = targetStepMs;
                rampStartedMs = now;
                rampDurationMs = rampMs;
            }
        }

        uint32_t steps = (now - lastStepMs) / currentStepMs;
        if (steps > WINDOW_COUNT)
        {
            lastStepMs += (steps - WINDOW_COUNT) * currentStepMs;
            steps = WINDOW_COUNT;
        }
        for (uint32_t step = 0; step < steps; ++step)
        {
            lastStepMs += currentStepMs;
            fades[position] = {lastStepMs, currentStepMs * 3 / 2, true};
            position = (position + 1) % WINDOW_COUNT;
            fades[position].active = false;
        }
        frameTimeMs = now;
    }

    uint8_t redAt(uint16_t pixel) const
    {
        const int window = windowOf(pixel);
        if (window < 0)
            return 0;
        if (window == static_cast<int>(position))
            return PEAK;
        const Fade &fade = fades[window];
        if (!fade.active || frameTimeMs - fade.startedMs >= fade.lengthMs)
            return 0;
        const uint32_t remaining = fade.lengthMs - (frameTimeMs - fade.startedMs);
        return static_cast<uint8_t>(PEAK * remaining / fade.lengthMs);
    }

    uint8_t greenAt(uint16_t pixel) const
    {
        return redAt(pixel) / 25;
    }

private:
    static constexpr uint16_t WINDOW_COUNT = 4;
    static constexpr uint8_t PEAK = 51;
    struct Fade
    {
        uint32_t startedMs;
        uint32_t lengthMs;
        bool active;
    };
    Fade fades[WINDOW_COUNT] = {};
    uint16_t position = 0;
    uint32_t frameTimeMs = 0;
    uint32_t lastStepMs = 0;
    uint32_t currentStepMs = config::CYCLOTRON_1984_START_STEP_MS;
    uint32_t rampFromMs = config::CYCLOTRON_1984_START_STEP_MS;
    uint32_t rampTargetMs = config::CYCLOTRON_1984_STEP_MS;
    uint32_t rampStartedMs = 0;
    uint32_t rampDurationMs = config::CYCLOTRON_1984_RAMP_MS;

    // Same lens windows as the Afterlife renderer.
    static int windowOf(uint16_t pixel)
    {
        if (pixel >= config::RING_PIXELS)
            return -1;
        for (uint16_t window = 0; window < WINDOW_COUNT; ++window)
        {
            const uint16_t start = (config::AFTERLIFE_WINDOW_OFFSET +
                                    window * config::RING_PIXELS / WINDOW_COUNT +
                                    config::RING_PIXELS - config::AFTERLIFE_WINDOW_PIXELS / 2) %
                                   config::RING_PIXELS;
            if ((pixel + config::RING_PIXELS - start) % config::RING_PIXELS < config::AFTERLIFE_WINDOW_PIXELS)
                return window;
        }
        return -1;
    }

    // Circular-out when speeding up (startup), circular-in when slowing (shutdown).
    void updateInterval(uint32_t now)
    {
        const uint32_t elapsed = now - rampStartedMs;
        if (elapsed >= rampDurationMs)
        {
            currentStepMs = rampTargetMs;
            return;
        }
        const float progress = static_cast<float>(elapsed) / rampDurationMs;
        const float eased = rampTargetMs < rampFromMs ?
                            sqrtf(1.0f - (1.0f - progress) * (1.0f - progress)) :
                            1.0f - sqrtf(1.0f - progress * progress);
        const float difference = static_cast<float>(rampTargetMs) - rampFromMs;
        currentStepMs = static_cast<uint32_t>(rampFromMs + difference * eased + 0.5f);
        if (currentStepMs == 0)
            currentStepMs = 1;
    }
};
