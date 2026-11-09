#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "WandController.h"

// Pack sound mute, toggled by holding fire for MUTE_HOLD_MS while Activate is off.
// The hold must start after fire was released with Activate off, so releasing
// Activate while still holding fire after firing never toggles it. The setting
// is saved in flash and sent to the pack in every link status packet.
class WandMute
{
public:
    void begin()
    {
        if (prefs.begin("wand", false)) // Read-write creates the namespace on first boot.
        {
            mutedState = prefs.getBool("muted", false);
            prefs.end();
        }
    }

    void update(const WandControls &controls, uint32_t now)
    {
        if (controls.activate)
        {
            freshPress = false;
            holding = false;
            return;
        }
        if (!controls.fire)
        {
            freshPress = true;
            holding = false;
            return;
        }
        if (!freshPress)
            return;
        if (!holding)
        {
            holding = true;
            holdStartedMs = now;
            return;
        }
        if (now - holdStartedMs >= wand_config::MUTE_HOLD_MS)
        {
            mutedState = !mutedState;
            freshPress = false; // Release before the next toggle.
            holding = false;
            save();
            Serial.printf("Sound %s (held fire %lu ms with Activate off)\n",
                          mutedState ? "MUTED" : "unmuted",
                          static_cast<unsigned long>(wand_config::MUTE_HOLD_MS));
        }
    }

    bool muted() const { return mutedState; }

    void printStatus() const
    {
        Serial.printf("Sound: %s; hold fire %lu s with Activate off to toggle\n",
                      mutedState ? "MUTED" : "on",
                      static_cast<unsigned long>(wand_config::MUTE_HOLD_MS / 1000));
    }

private:
    Preferences prefs;
    bool mutedState = false;
    bool freshPress = false;
    bool holding = false;
    uint32_t holdStartedMs = 0;

    void save()
    {
        if (prefs.begin("wand", false))
        {
            prefs.putBool("muted", mutedState);
            prefs.end();
        }
        else
            Serial.println("Sound mute: could not save setting to flash");
    }
};
