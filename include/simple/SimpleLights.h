#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Cyclotron.h"
#include "PackController.h"
#include "WandController.h"
#include "SimpleConfig.h"

// Simple pack lights: cyclotron jewels, power cell and the wand chain.
// The cyclotron reuses the main pack's AfterlifeCyclotron and Cyclotron1984
// renderers unchanged: they animate the main 40-pixel ring's four 6-pixel lens
// windows, and each window is shown on one jewel's outer ring. Speeds and ramps
// follow PackLights::update() in the main pack.
class SimpleLights
{
public:
    void begin(const PackController &pack, uint32_t now)
    {
        cyclotronPixels.begin();
        cyclotronPixels.setBrightness(simple_config::CYCLOTRON_BRIGHTNESS);
        powerCell.begin();
        powerCell.setBrightness(simple_config::POWER_CELL_BRIGHTNESS);
        wandPixels.begin();
        wandPixels.setBrightness(simple_config::WAND_BRIGHTNESS);
        blank();
        lastTheme = pack.theme();
        lastState = pack.state();
        powerCellState = pack.state();
        powerCellStepStarted = now;
        cyclotron84.begin(now);
        afterlife.begin(now);
    }

    void blank()
    {
        cyclotronPixels.clear();
        cyclotronPixels.show();
        powerCell.clear();
        powerCell.show();
        wandPixels.clear();
        wandPixels.show();
    }

    void update(const PackController &pack, const WandController &wand, uint8_t speedLevel, uint32_t now)
    {
        if (now - lastPackFrameMs >= config::FRAME_INTERVAL_MS)
        {
            lastPackFrameMs = now;
            updatePack(pack, speedLevel, now);
        }
        updateWand(wand, pack.theme(), now);
    }

private:
    Adafruit_NeoPixel cyclotronPixels{simple_config::CYCLOTRON_PIXELS, simple_config::CYCLOTRON_PIN,
                                      NEO_GRB + NEO_KHZ800};
    Adafruit_NeoPixel powerCell{simple_config::POWER_CELL_PIXELS, simple_config::POWER_CELL_PIN,
                                NEO_GRB + NEO_KHZ800};
    Adafruit_NeoPixel wandPixels{simple_config::WAND_PIXELS, simple_config::WAND_PIN, NEO_GRB + NEO_KHZ800};

    // Pack (cyclotron + power cell).
    Cyclotron1984 cyclotron84;
    AfterlifeCyclotron afterlife;
    uint32_t lastPackFrameMs = 0;
    SystemTheme lastTheme = config::SYSTEM_THEME;
    PackState lastState = PackState::Off;
    uint8_t currentSpeedLevel = 1;
    uint16_t powerCellLit = 0;
    uint32_t powerCellStepStarted = 0;
    PackState powerCellState = PackState::Off;

    // Wand.
    uint32_t lastWandFrameMs = 0;
    WandState lastWandState = WandState::Off;
    enum class BarPhase : uint8_t { Activation, Recovery, Idle };
    BarPhase barPhase = BarPhase::Idle;
    uint32_t barPhaseStartMs = 0;
    uint32_t offStartMs = 0;
    bool offDraining = false;

    void updatePack(const PackController &pack, uint8_t speedLevel, uint32_t now)
    {
        currentSpeedLevel = pack.state() == PackState::Firing ? speedLevel : 1;
        const bool immediateSpeed = pack.state() != PackState::Stopping &&
                                    (pack.state() == PackState::Firing || lastState == PackState::Firing ||
                                     pack.state() == PackState::Cooling);
        if (pack.theme() != lastTheme ||
            (pack.state() == PackState::Starting && lastState != PackState::Starting))
        {
            cyclotron84.begin(now);
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
                const uint8_t relativeStep = currentSpeedLevel > 1 ? 7 - currentSpeedLevel : 7;
                targetStepMs = (config::AFTERLIFE_IDLE_STEP_MS * relativeStep + 3) / 7;
            }
            else if (pack.state() == PackState::Stopping)
                targetStepMs = config::AFTERLIFE_START_STEP_MS;
            if (pack.state() != PackState::Off)
                afterlife.update(now, targetStepMs, immediateSpeed,
                                 pack.state() == PackState::Stopping ? shutdownRampMs : config::AFTERLIFE_RAMP_MS);
        }
        else
        {
            uint32_t stepMs = config::CYCLOTRON_1984_STEP_MS;
            uint32_t rampMs = config::CYCLOTRON_1984_RAMP_MS;
            if (pack.state() == PackState::Firing)
                stepMs = stepMs / currentSpeedLevel;
            else if (pack.state() == PackState::Stopping)
            {
                stepMs = config::CYCLOTRON_1984_START_STEP_MS;
                rampMs = shutdownRampMs;
            }
            if (pack.state() != PackState::Off)
                cyclotron84.update(now, stepMs, immediateSpeed, rampMs);
        }

        const float scale = brightnessScale(pack, now);
        renderCyclotron(pack, scale);
        renderPowerCell(pack, now, scale);
    }

    // Same as PackLights::brightnessScale() in the main pack.
    static float brightnessScale(const PackController &pack, uint32_t now)
    {
        switch (pack.state())
        {
        case PackState::Off:
            return 0.0f;
        case PackState::Starting:
            return min(1.0f, static_cast<float>(pack.stateAge(now)) / config::STARTUP_MS);
        case PackState::Stopping:
        {
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

    // Main-ring pixel shown at (lens window, position) by the shared renderers.
    static uint16_t ringPixel(uint16_t window, uint16_t position)
    {
        return (config::AFTERLIFE_WINDOW_OFFSET + window * config::RING_PIXELS / 4 + config::RING_PIXELS -
                config::AFTERLIFE_WINDOW_PIXELS / 2 + position) %
               config::RING_PIXELS;
    }

    static uint8_t level(uint8_t base, float scale)
    {
        const float value = base * scale * simple_config::CYCLOTRON_GAIN_PERCENT / 100.0f + 0.5f;
        return static_cast<uint8_t>(value > 255.0f ? 255.0f : value);
    }

    void renderCyclotron(const PackController &pack, float scale)
    {
        static_assert(config::AFTERLIFE_WINDOW_PIXELS == simple_config::JEWEL_PIXELS - 1,
                      "Each jewel's outer ring shows one 6-pixel lens window");
        const bool isAfterlife = pack.cyclotronMode() == CyclotronMode::Afterlife;
        constexpr uint16_t outerFirst = simple_config::JEWEL_CENTER_FIRST ? 1 : 0;
        constexpr uint16_t center = simple_config::JEWEL_CENTER_FIRST ? 0 : simple_config::JEWEL_PIXELS - 1;
        for (uint16_t jewel = 0; jewel < simple_config::JEWELS; ++jewel)
        {
            const uint16_t base = jewel * simple_config::JEWEL_PIXELS;
            uint8_t centerRed = 0;
            uint8_t centerGreen = 0;
            for (uint16_t position = 0; position < config::AFTERLIFE_WINDOW_PIXELS; ++position)
            {
                const uint16_t pixel = ringPixel(jewel, position);
                const uint8_t redLevel = isAfterlife ? afterlife.redAt(pixel) : cyclotron84.redAt(pixel);
                const uint8_t greenLevel = isAfterlife ? afterlife.greenAt(pixel) : cyclotron84.greenAt(pixel);
                const uint8_t red = level(redLevel, scale);
                const uint8_t green = level(greenLevel, scale);
                cyclotronPixels.setPixelColor(base + outerFirst + position, cyclotronPixels.Color(red, green, 0));
                if (red > centerRed)
                {
                    centerRed = red;
                    centerGreen = green;
                }
            }
            cyclotronPixels.setPixelColor(base + center, cyclotronPixels.Color(centerRed, centerGreen, 0));
        }
        cyclotronPixels.show();
    }

    // Same as PackLights::renderPowerBar() in the main pack, for 12 pixels.
    void renderPowerCell(const PackController &pack, uint32_t now, float scale)
    {
        constexpr uint16_t pixels = simple_config::POWER_CELL_PIXELS;
        uint32_t stepMs = config::POWER_BAR_STEP_MS;
        if (pack.state() == PackState::Firing)
        {
            const uint32_t reduction = 5 * (currentSpeedLevel - 1);
            stepMs = stepMs > reduction ? stepMs - reduction : 1;
        }
        const bool afterlifeRamp = pack.cyclotronMode() == CyclotronMode::Afterlife &&
                                   pack.state() != PackState::Firing &&
                                   afterlife.stepIntervalMs() > config::AFTERLIFE_IDLE_STEP_MS;
        if (afterlifeRamp)
            stepMs += afterlife.stepIntervalMs();

        if (powerCellState != pack.state())
        {
            const bool continues = pack.cyclotronMode() == CyclotronMode::Afterlife &&
                                   ((powerCellState == PackState::Starting && pack.state() == PackState::Idle) ||
                                    pack.state() == PackState::Stopping);
            if (!continues)
                powerCellLit = 0;
            powerCellStepStarted = now;
            powerCellState = pack.state();
        }
        const uint32_t steps = (now - powerCellStepStarted) / stepMs;
        powerCellStepStarted += steps * stepMs;
        powerCellLit = (powerCellLit + steps % (pixels + 1)) % (pixels + 1);
        uint16_t litPixels = powerCellLit;
        if ((pack.state() == PackState::Starting || pack.state() == PackState::Stopping) &&
            pack.cyclotronMode() != CyclotronMode::Afterlife)
            litPixels = static_cast<uint16_t>(pixels * scale);
        else if (pack.state() == PackState::Cooling)
            litPixels = pixels;
        else if (pack.state() == PackState::Off)
            litPixels = 0;

        powerCell.clear();
        const uint8_t blue = static_cast<uint8_t>(255.0f * scale + 0.5f);
        for (uint16_t pixel = 0; pixel < litPixels; ++pixel)
            powerCell.setPixelColor(pixel, powerCell.Color(0, 0, blue));
        powerCell.show();
    }

    // ---- Wand: mini bargraph, body lights, vent and firing strip ----

    static constexpr uint16_t BAR = simple_config::BARGRAPH_PIXELS;
    // The main 28-segment bargraph timings scaled to 4 pixels (7 segments each).
    static constexpr uint32_t BAR_ORIGINAL_STEP_MS = 64 * 7;      // Afterlife/FE fill
    static constexpr uint32_t BAR_SWEEP_ACTIVATION_MS = 24 * 7;   // 1984/1989 first sweep
    static constexpr uint32_t BAR_SWEEP_STEP_MS = 12 * 7;         // 1984/1989 idle sweep
    static constexpr uint32_t BAR_PAUSE_MS = 90;
    static constexpr uint32_t BAR_DRAIN_STEP_MS = 80 * 7;         // overheat and Activate off
    static constexpr uint32_t BAR_FIRING_STEP_MS = 15 * 13 / 2;   // centre <-> ends at 15 ms/segment

    static uint32_t scaled(uint32_t color, uint8_t percent)
    {
        const uint32_t r = ((color >> 16) & 0xFF) * percent / 100;
        const uint32_t g = ((color >> 8) & 0xFF) * percent / 100;
        const uint32_t b = (color & 0xFF) * percent / 100;
        return (r << 16) | (g << 8) | b;
    }

    static uint32_t barFilled(uint16_t count)
    {
        return count >= 32 ? 0xFFFFFFFF : (1UL << count) - 1;
    }

    static uint32_t barDrain(uint32_t sinceMs)
    {
        const uint32_t steps = sinceMs / BAR_DRAIN_STEP_MS;
        return steps >= BAR ? 0 : barFilled(static_cast<uint16_t>(BAR - steps));
    }

    static uint32_t barSweep(uint32_t t, uint32_t stepMs, bool leadingPause)
    {
        if (leadingPause)
        {
            if (t < BAR_PAUSE_MS)
                return 0;
            t -= BAR_PAUSE_MS;
        }
        const uint32_t ramp = BAR * stepMs;
        if (t < ramp)
            return barFilled(static_cast<uint16_t>(1 + t / stepMs));
        t -= ramp;
        if (t < BAR_PAUSE_MS)
            return barFilled(BAR);
        t -= BAR_PAUSE_MS;
        if (t < ramp)
            return barFilled(static_cast<uint16_t>(BAR - 1 - t / stepMs));
        return 0;
    }

    uint32_t barIdle(bool superHero, uint32_t now) const
    {
        const uint32_t t = now - barPhaseStartMs;
        if (!superHero)
        {
            if (barPhase == BarPhase::Idle)
                return barFilled(BAR);
            const uint32_t count = t / BAR_ORIGINAL_STEP_MS + 1;
            return barFilled(static_cast<uint16_t>(count > BAR ? BAR : count));
        }
        uint32_t loopT = t;
        if (barPhase != BarPhase::Idle)
        {
            const uint32_t firstStep = barPhase == BarPhase::Activation ? BAR_SWEEP_ACTIVATION_MS : BAR_SWEEP_STEP_MS;
            const uint32_t firstLength = 2 * BAR * firstStep + BAR_PAUSE_MS;
            if (t < firstLength)
                return barSweep(t, firstStep, false);
            loopT = t - firstLength;
        }
        const uint32_t loopLength = 2 * BAR * BAR_SWEEP_STEP_MS + 2 * BAR_PAUSE_MS;
        return barSweep(loopT % loopLength, BAR_SWEEP_STEP_MS, true);
    }

    // Super Hero firing: a pair moves centre -> ends -> centre, faster near overheat.
    static uint32_t barFiring(uint32_t elapsed)
    {
        const uint32_t total = wand_config::MAX_FIRE_MS;
        const uint32_t remaining = elapsed < total ? total - elapsed : 0;
        const uint32_t stepMs = remaining < total / 6 ? BAR_FIRING_STEP_MS * 7 / 15 :
                                remaining < total / 5 ? BAR_FIRING_STEP_MS * 9 / 15 :
                                remaining < total / 4 ? BAR_FIRING_STEP_MS * 11 / 15 :
                                remaining < total / 3 ? BAR_FIRING_STEP_MS * 13 / 15 : BAR_FIRING_STEP_MS;
        constexpr uint16_t maxDistance = BAR / 2 - 1;
        const uint16_t cycle = maxDistance * 2;
        const uint16_t step = cycle == 0 ? 0 : (elapsed / stepMs) % cycle;
        const uint16_t distance = step <= maxDistance ? step : cycle - step;
        return (1UL << (maxDistance - distance)) | (1UL << (maxDistance + 1 + distance));
    }

    void trackWandTransitions(const WandController &wand, uint32_t now)
    {
        const WandState state = wand.state();
        if (state == lastWandState)
            return;
        const bool ready = state == WandState::Idle || state == WandState::Ready;
        if (state == WandState::Starting)
        {
            barPhase = BarPhase::Activation;
            barPhaseStartMs = now;
        }
        else if (ready && lastWandState == WandState::Cooling)
        {
            barPhase = BarPhase::Recovery;
            barPhaseStartMs = now;
        }
        else if (ready && lastWandState == WandState::Firing)
        {
            barPhase = BarPhase::Idle;
            barPhaseStartMs = now;
        }
        else if (state == WandState::Off)
        {
            offStartMs = now;
            offDraining = true;
        }
        lastWandState = state;
    }

    uint32_t barFrame(const WandController &wand, SystemTheme theme, uint32_t now) const
    {
        const bool superHero = !config::themeIsModern(theme);
        switch (wand.state())
        {
        case WandState::Off:
            return offDraining ? barDrain(now - offStartMs) : 0;
        case WandState::Starting:
        case WandState::Idle:
        case WandState::Ready:
            return barIdle(superHero, now);
        case WandState::Firing:
            return barFiring(wand.elapsed(now));
        case WandState::Cooling:
            return barDrain(wand.elapsed(now));
        }
        return 0;
    }

    void updateWand(const WandController &wand, SystemTheme theme, uint32_t now)
    {
        const uint32_t frameMs = wand.state() == WandState::Firing ? wand_config::FIRING_FRAME_INTERVAL_MS
                                                                    : wand_config::FRAME_INTERVAL_MS;
        trackWandTransitions(wand, now);
        if (now - lastWandFrameMs < frameMs)
            return;
        lastWandFrameMs = now;

        wandPixels.clear();
        uint8_t brightness = simple_config::WAND_BRIGHTNESS;
        if (wand.state() == WandState::Starting && wand.elapsed(now) < wand_config::STARTUP_MS)
            brightness = static_cast<uint8_t>(simple_config::WAND_BRIGHTNESS * wand.elapsed(now) /
                                              wand_config::STARTUP_MS);
        wandPixels.setBrightness(brightness);

        // The bargraph keeps its own drain after Activate off at full brightness.
        const uint32_t bar = barFrame(wand, theme, now);
        if (wand.state() == WandState::Off)
            wandPixels.setBrightness(simple_config::WAND_BRIGHTNESS);
        for (uint16_t segment = 0; segment < BAR; ++segment)
        {
            if ((bar & (1UL << segment)) == 0)
                continue;
            const uint16_t pixel = simple_config::BARGRAPH_FIRST +
                                   (simple_config::BARGRAPH_FIRST_IS_BOTTOM ? segment : BAR - 1 - segment);
            wandPixels.setPixelColor(pixel, scaled(simple_config::BARGRAPH_COLOR, simple_config::BARGRAPH_LEVEL_PERCENT));
        }

        if (wand.state() != WandState::Off)
        {
            // Main wand: overheat blinks slo-blo/vent against the white body light.
            const bool cooling = wand.state() == WandState::Cooling;
            const bool blinkOn = (wand.elapsed(now) / 250) % 2 == 0;
            if (!cooling || blinkOn)
            {
                wandPixels.setPixelColor(simple_config::BODY_1_PIXEL, simple_config::BODY_1_COLOR);
                wandPixels.setPixelColor(simple_config::VENT_PIXEL, simple_config::VENT_COLOR);
            }
            if (!cooling || !blinkOn)
                wandPixels.setPixelColor(simple_config::BODY_2_PIXEL, simple_config::BODY_2_COLOR);

            if (wand.state() == WandState::Firing)
                renderFiring(wand.elapsed(now));
        }
        wandPixels.show();
    }

    // Main wand firing effect on 3 pixels: red/blue strobe with a contrasting
    // random spark, and the tip (last pixel) flashing white.
    void renderFiring(uint32_t elapsed)
    {
        const bool strobeOn = elapsed % wand_config::FIRING_STROBE_PERIOD_MS < wand_config::FIRING_STROBE_ON_MS;
        const bool alternate = (elapsed / wand_config::FIRING_STROBE_PERIOD_MS) % 2 != 0;
        constexpr uint16_t tip = simple_config::FIRING_FIRST + simple_config::FIRING_PIXELS - 1;
        if (strobeOn)
        {
            wandPixels.fill(alternate ? simple_config::STREAM_ALT_COLOR : simple_config::STREAM_COLOR,
                            simple_config::FIRING_FIRST, simple_config::FIRING_PIXELS);
            const uint16_t spark = simple_config::FIRING_FIRST + static_cast<uint16_t>(random(simple_config::FIRING_PIXELS));
            wandPixels.setPixelColor(spark, alternate ? simple_config::STREAM_COLOR : simple_config::STREAM_ALT_COLOR);
        }
        const bool tipOn = elapsed % wand_config::FIRING_TIP_PERIOD_MS < wand_config::FIRING_TIP_FLASH_MS;
        if (tipOn)
            wandPixels.setPixelColor(tip, simple_config::STREAM_TIP_COLOR);
    }
};
