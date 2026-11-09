#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <ht16k33.h>
#include "WandController.h"
#include "WandPackLink.h"

class WandBargraph
{
public:
    void begin()
    {
        if (!Wire.begin(wand_config::BARGRAPH_SDA_PIN, wand_config::BARGRAPH_SCL_PIN, 100000))
        {
            statusText = "I2C initialization failed; wand remains available";
            return;
        }
        Wire.setTimeOut(10);
        if (digitalRead(wand_config::BARGRAPH_SDA_PIN) == LOW ||
            digitalRead(wand_config::BARGRAPH_SCL_PIN) == LOW)
        {
            statusText = "I2C bus held LOW; check wiring, then reset";
            return;
        }

        Wire.beginTransmission(wand_config::BARGRAPH_I2C_ADDRESS);
        lastError = Wire.endTransmission();
        if (lastError != 0)
        {
            statusText = "no acknowledgement at configured address; check power/wiring, then reset";
            return;
        }

        constexpr uint8_t oscillatorOn = 0x21;
        constexpr uint8_t displayOff = 0x80;
        const uint8_t startupCommands[] = {oscillatorOn, displayOff};
        for (uint8_t command : startupCommands)
        {
            Wire.beginTransmission(wand_config::BARGRAPH_I2C_ADDRESS);
            Wire.write(command);
            lastError = Wire.endTransmission();
            if (lastError != 0)
            {
                statusText = "HT16K33 wake/display-off command failed; reset to retry";
                return;
            }
        }

        const uint8_t blank[16] = {};
        Wire.beginTransmission(wand_config::BARGRAPH_I2C_ADDRESS);
        Wire.write(static_cast<uint8_t>(0x00));
        Wire.write(blank, sizeof(blank));
        lastError = Wire.endTransmission();
        if (lastError != 0)
        {
            statusText = "address acknowledged, but display RAM clear after wake failed; reset to retry";
            return;
        }

        display.begin(wand_config::BARGRAPH_I2C_ADDRESS - 0x70);
        lastError = display.setBrightness(wand_config::BARGRAPH_BRIGHTNESS);
        if (lastError != 0)
        {
            display.displayOff();
            statusText = "brightness setup failed; reset to retry";
            return;
        }
        available = true;
        statusText = wand_config::BARGRAPH_DIAGNOSTIC_MODE ?
                     "diagnostic sweep ready; use Activate to start/stop" :
                     "Afterlife/Super Hero animations ready";
    }

    // theme: wand_link::Theme from the pack (1984/1989 use GPStar's Super Hero
    // bargraph mode, Afterlife/Frozen Empire its Original mode).
    void update(const WandController &wand, uint8_t theme, uint32_t now)
    {
        if (!available)
            return;
        superHero = theme == wand_link::Theme1984 || theme == wand_link::Theme1989;
        trackTransitions(wand, now);

        uint32_t nextFrame = 0;
        if (wand_config::BARGRAPH_DIAGNOSTIC_MODE)
        {
            if (wand.state() != WandState::Off)
            {
                if (!diagnosticRunning)
                {
                    diagnosticStarted = now;
                    diagnosticRunning = true;
                }
                constexpr uint32_t sweepMs =
                    wand_config::BARGRAPH_SEGMENTS * wand_config::BARGRAPH_DIAGNOSTIC_STEP_MS;
                const uint32_t position =
                    (now - diagnosticStarted) % (sweepMs + wand_config::BARGRAPH_DIAGNOSTIC_BLANK_MS);
                if (position < sweepMs)
                    nextFrame = segmentBit(position / wand_config::BARGRAPH_DIAGNOSTIC_STEP_MS);
            }
            else
            {
                diagnosticRunning = false;
            }
        }
        else
        {
            nextFrame = animationFrame(wand, now);
        }

        if (nextFrame == visibleFrame)
            return;

        for (uint8_t &value : display.displayRam)
            value = 0;
        for (uint8_t segment = 0; segment < wand_config::BARGRAPH_SEGMENTS; ++segment)
        {
            if ((nextFrame & segmentBit(segment)) != 0)
                display.setLed(segment / 4 + 16 * (segment % 4));
        }

        lastError = display.sendLed();
        if (lastError != 0)
        {
            display.displayOff();
            available = false;
            statusText = "I2C write failed; bargraph stopped, reset to retry";
            printStatus();
            return;
        }
        visibleFrame = nextFrame;
    }

    void printStatus() const
    {
        Serial.printf("Bargraph: SDA=%d SCL=%d address=0x%02X, %u segments, brightness=%u/15\n",
                      static_cast<int>(wand_config::BARGRAPH_SDA_PIN),
                      static_cast<int>(wand_config::BARGRAPH_SCL_PIN),
                      static_cast<unsigned>(wand_config::BARGRAPH_I2C_ADDRESS),
                      static_cast<unsigned>(wand_config::BARGRAPH_SEGMENTS),
                      static_cast<unsigned>(wand_config::BARGRAPH_BRIGHTNESS));
        Serial.printf("Bargraph status: %s (I2C error %u)\n", statusText, static_cast<unsigned>(lastError));
    }

private:
    static constexpr uint32_t segmentBit(uint8_t segment)
    {
        return static_cast<uint32_t>(1) << segment;
    }

    static constexpr uint32_t filledSegments(uint8_t count)
    {
        return count >= wand_config::BARGRAPH_SEGMENTS ?
               (static_cast<uint32_t>(1) << wand_config::BARGRAPH_SEGMENTS) - 1 :
               (static_cast<uint32_t>(1) << count) - 1;
    }

    // GPStar 28-segment timings (NeutronaWand Header.h): i_bargraph_interval 4 ms x
    // multipliers 16 (Original ramp), 6 (Super Hero activation), 3 (Super Hero idle);
    // i_bargraph_wait / 2 = 90 ms pauses.
    static constexpr uint32_t ORIGINAL_RAMP_MS = 64;
    static constexpr uint32_t SUPER_HERO_ACTIVATION_MS = 24;
    static constexpr uint32_t SUPER_HERO_IDLE_MS = 12;
    static constexpr uint32_t PAUSE_MS = 90;
    static constexpr uint8_t N = wand_config::BARGRAPH_SEGMENTS;

    enum class Phase : uint8_t { Activation, Recovery, Idle };

    // Restart the idle animation at the moments GPStar does.
    void trackTransitions(const WandController &wand, uint32_t now)
    {
        const WandState state = wand.state();
        if (state == lastState)
            return;
        const bool idleState = state == WandState::Idle || state == WandState::Ready;
        if (state == WandState::Starting)
        {
            phase = Phase::Activation;
            phaseStartMs = now;
        }
        else if (idleState && lastState == WandState::Cooling)
        {
            phase = Phase::Recovery;          // overheatingFinished(): ramp back up
            phaseStartMs = now;
        }
        else if (idleState && lastState == WandState::Firing)
        {
            phase = Phase::Idle;              // resume the power-level display
            phaseStartMs = now;
        }
        else if (state == WandState::Off && lastState != WandState::Off)
        {
            offStartMs = now;                 // wandOff(): full, then drain top-down
            offDraining = true;
        }
        lastState = state;
    }

    static uint32_t drainFrame(uint32_t sinceMs)
    {
        const uint32_t steps = sinceMs / wand_config::BARGRAPH_COOLDOWN_STEP_MS;
        return steps >= N ? 0 : filledSegments(static_cast<uint8_t>(N - steps));
    }

    // Super Hero up/down sweep: up one segment per stepMs, pause, down, pause.
    static uint32_t sweepFrame(uint32_t t, uint32_t stepMs, bool leadingPause)
    {
        if (leadingPause)
        {
            if (t < PAUSE_MS)
                return 0;
            t -= PAUSE_MS;
        }
        const uint32_t ramp = N * stepMs;
        if (t < ramp)
            return filledSegments(static_cast<uint8_t>(1 + t / stepMs));
        t -= ramp;
        if (t < PAUSE_MS)
            return filledSegments(N);
        t -= PAUSE_MS;
        if (t < ramp)
            return filledSegments(static_cast<uint8_t>(N - 1 - t / stepMs));
        return 0;
    }

    uint32_t idleFrame(uint32_t now) const
    {
        const uint32_t t = now - phaseStartMs;
        if (!superHero)
        {
            // Original mode at level 5: slow fill (activation and recovery), then full.
            if (phase == Phase::Idle)
                return filledSegments(N);
            const uint32_t count = t / ORIGINAL_RAMP_MS + 1;
            return filledSegments(static_cast<uint8_t>(count > N ? N : count));
        }
        // Super Hero: one up/down ramp (slower on activation), then a continuous
        // bottom-top-bottom sweep with pauses at both ends.
        uint32_t loopT = t;
        if (phase != Phase::Idle)
        {
            const uint32_t firstStep = phase == Phase::Activation ? SUPER_HERO_ACTIVATION_MS : SUPER_HERO_IDLE_MS;
            const uint32_t firstLength = 2 * N * firstStep + PAUSE_MS;
            if (t < firstLength)
                return sweepFrame(t, firstStep, false);
            loopT = t - firstLength;
        }
        const uint32_t loopLength = 2 * N * SUPER_HERO_IDLE_MS + 2 * PAUSE_MS;
        return sweepFrame(loopT % loopLength, SUPER_HERO_IDLE_MS, true);
    }

    // bargraphRampFiring(): GPStar Super Hero firing animation for all themes. A
    // symmetric pair moves centre -> ends -> centre, 15 ms per step at level 5,
    // speeding up as less of the overheat timer remains.
    static uint32_t firingFrame(uint32_t elapsed)
    {
        const uint32_t total = wand_config::MAX_FIRE_MS;
        const uint32_t remaining = elapsed < total ? total - elapsed : 0;
        const uint32_t stepMs = remaining < total / 6 ? 7 : remaining < total / 5 ? 9 :
                                remaining < total / 4 ? 11 : remaining < total / 3 ? 13 :
                                wand_config::BARGRAPH_FIRING_STEP_MS;
        constexpr uint8_t maximumDistance = N / 2 - 1;
        constexpr uint8_t cycleSteps = maximumDistance * 2;
        // Phase from elapsed time at the current step; small jumps when the step
        // changes are invisible at these speeds.
        const uint8_t step = (elapsed / stepMs) % cycleSteps;
        const uint8_t distance = step <= maximumDistance ? step : cycleSteps - step;
        return segmentBit(maximumDistance - distance) | segmentBit(maximumDistance + 1 + distance);
    }

    uint32_t animationFrame(const WandController &wand, uint32_t now) const
    {
        switch (wand.state())
        {
        case WandState::Off:
            return offDraining ? drainFrame(now - offStartMs) : 0;
        case WandState::Starting:
        case WandState::Idle:
        case WandState::Ready:
            return idleFrame(now);
        case WandState::Firing:
            return firingFrame(wand.elapsed(now));
        case WandState::Cooling:
            // Overheat (prepBargraphRampDown): full, then drain top-down at 80 ms.
            return drainFrame(wand.elapsed(now));
        }
        return 0;
    }

    HT16K33 display;
    bool available = false;
    bool diagnosticRunning = false;
    uint32_t diagnosticStarted = 0;
    uint32_t visibleFrame = 0;
    bool superHero = false;
    WandState lastState = WandState::Off;
    Phase phase = Phase::Idle;
    uint32_t phaseStartMs = 0;
    uint32_t offStartMs = 0;
    bool offDraining = false;
    uint8_t lastError = 0;
    const char *statusText = "not initialized";
};