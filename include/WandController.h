#pragma once

#include "WandConfig.h"

enum class WandState
{
    Off,
    Starting,
    Idle,
    Ready,
    Firing,
    Cooling
};

struct WandControls
{
    bool activate = false;
    bool vent = false;
    bool wand = false;
    bool fire = false;
};

class WandController
{
public:
    void begin(uint32_t now)
    {
        currentState = wand_config::BENCH_IDLE_FIRING_MODE ? WandState::Idle : WandState::Off;
        stateStarted = now;
        fireHeld = wand_config::BENCH_IDLE_FIRING_MODE;
        benchCooling = false;
    }

    void update(const WandControls &controls, uint32_t now)
    {
        const bool firePressed = controls.fire && !fireHeld;
        fireHeld = controls.fire;

        if (wand_config::BENCH_IDLE_FIRING_MODE)
        {
            if (currentState == WandState::Firing)
            {
                if (elapsed(now) >= wand_config::MAX_FIRE_MS)
                {
                    benchCooling = true;
                    transition(WandState::Idle, now);
                }
                else if (!controls.fire)
                {
                    transition(WandState::Idle, now);
                }
            }
            else
            {
                if (benchCooling && elapsed(now) >= wand_config::COOLDOWN_MS)
                    benchCooling = false;
                if (!benchCooling && firePressed)
                    transition(WandState::Firing, now);
            }
            return;
        }

        const bool enabled = controls.vent && controls.wand;
        const WandState restingState = enabled ? WandState::Ready : WandState::Idle;

        if (!controls.activate)
        {
            transition(WandState::Off, now);
            return;
        }

        switch (currentState)
        {
        case WandState::Off:
            transition(WandState::Starting, now);
            break;
        case WandState::Starting:
            if (elapsed(now) >= wand_config::STARTUP_MS)
                transition(restingState, now);
            break;
        case WandState::Idle:
            transition(restingState, now);
            break;
        case WandState::Ready:
            if (!enabled)
                transition(WandState::Idle, now);
            else if (firePressed)
                transition(WandState::Firing, now);
            break;
        case WandState::Firing:
            if (elapsed(now) >= wand_config::MAX_FIRE_MS)
                transition(WandState::Cooling, now);
            else if (!enabled || !controls.fire)
                transition(restingState, now);
            break;
        case WandState::Cooling:
            if (elapsed(now) >= wand_config::COOLDOWN_MS)
                transition(restingState, now);
            break;
        }
    }

    WandState state() const { return currentState; }
    bool cooling() const { return currentState == WandState::Cooling || benchCooling; }
    uint32_t elapsed(uint32_t now) const { return now - stateStarted; }

    uint8_t firingSpeedLevel(uint32_t now) const
    {
        if (currentState != WandState::Firing)
            return 1;

        const uint32_t age = elapsed(now);
        const uint32_t remaining = age < wand_config::MAX_FIRE_MS ?
                                   wand_config::MAX_FIRE_MS - age : 0;
        if (remaining < wand_config::MAX_FIRE_MS / 4)
            return 4;
        if (remaining < wand_config::MAX_FIRE_MS / 3)
            return 3;
        if (remaining < wand_config::MAX_FIRE_MS / 2)
            return 2;
        return 1;
    }

    const char *stateName() const
    {
        switch (currentState)
        {
        case WandState::Off:
            return "Off";
        case WandState::Starting:
            return "Starting";
        case WandState::Idle:
            return wand_config::BENCH_IDLE_FIRING_MODE ? "Idle" : "Idle (firing gates off)";
        case WandState::Ready:
            return "Ready";
        case WandState::Firing:
            return "Firing";
        case WandState::Cooling:
            return "Cooling (lights only)";
        }
        return "Unknown";
    }

private:
    WandState currentState = WandState::Off;
    uint32_t stateStarted = 0;
    bool fireHeld = false;
    bool benchCooling = false;

    void transition(WandState next, uint32_t now)
    {
        if (next != currentState)
        {
            currentState = next;
            stateStarted = now;
        }
    }
};