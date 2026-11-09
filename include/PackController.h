#pragma once

#include "Config.h"

enum class PackState
{
    Off,
    Starting,
    Idle,
    Firing,
    Cooling,
    Stopping
};

class PackController
{
public:
    void begin(uint32_t now, bool autoStart)
    {
        fireHeld = false;
        currentTheme = config::SYSTEM_THEME;
        transition(autoStart ? PackState::Idle : PackState::Off, now);
    }

    // The cyclotron style follows the system theme (1984/1989 or Afterlife/Frozen Empire).
    CyclotronMode cyclotronMode() const
    {
        return config::themeCyclotronMode(currentTheme);
    }

    void setCyclotronMode(CyclotronMode mode)
    {
        if (mode != cyclotronMode())
            currentTheme = mode == CyclotronMode::Afterlife ? SystemTheme::Afterlife : SystemTheme::Gb1984;
    }

    const char *cyclotronModeName() const
    {
        return config::themeName(currentTheme);
    }

    SystemTheme theme() const
    {
        return currentTheme;
    }

    void setTheme(SystemTheme theme)
    {
        currentTheme = theme;
    }

    PackState state() const
    {
        return currentState;
    }

    uint32_t stateAge(uint32_t now) const
    {
        return now - stateStartedMs;
    }

    bool isOn() const
    {
        return currentState != PackState::Off && currentState != PackState::Stopping;
    }

    void setPower(bool enabled, uint32_t now)
    {
        if (enabled && !isOn())
        {
            transition(PackState::Starting, now);
        }
        else if (!enabled && isOn())
        {
            transition(PackState::Stopping, now);
        }
    }

    void togglePower(uint32_t now)
    {
        setPower(!isOn(), now);
    }

    void setFiring(bool pressed, uint32_t now)
    {
        const bool newlyPressed = pressed && !fireHeld;
        fireHeld = pressed;

        if (!pressed && currentState == PackState::Firing)
        {
            transition(PackState::Idle, now);
        }
        else if (newlyPressed && currentState == PackState::Idle)
        {
            transition(PackState::Firing, now);
        }
    }

    void vent(uint32_t now)
    {
        if (currentState == PackState::Idle || currentState == PackState::Firing)
        {
            transition(PackState::Cooling, now);
        }
    }

    void update(uint32_t now)
    {
        const uint32_t elapsed = stateAge(now);
        switch (currentState)
        {
        case PackState::Starting:
            if (elapsed >= config::STARTUP_MS)
                transition(PackState::Idle, now);
            break;
        case PackState::Firing:
            if (elapsed >= config::MAX_FIRE_MS)
                transition(PackState::Cooling, now);
            break;
        case PackState::Cooling:
            if (elapsed >= config::COOLDOWN_MS)
                transition(PackState::Idle, now);
            break;
        case PackState::Stopping:
            if (elapsed >= config::themeShutdownMs(currentTheme))
                transition(PackState::Off, now);
            break;
        default:
            break;
        }
    }

    const char *stateName() const
    {
        switch (currentState)
        {
        case PackState::Off:
            return "Off";
        case PackState::Starting:
            return "Starting";
        case PackState::Idle:
            return "Idle";
        case PackState::Firing:
            return "Firing";
        case PackState::Cooling:
            return "Cooling";
        case PackState::Stopping:
            return "Stopping";
        }
        return "Unknown";
    }

private:
    PackState currentState = PackState::Off;
    SystemTheme currentTheme = config::SYSTEM_THEME;
    uint32_t stateStartedMs = 0;
    bool fireHeld = false;

    void transition(PackState nextState, uint32_t now)
    {
        currentState = nextState;
        stateStartedMs = now;
    }
};
