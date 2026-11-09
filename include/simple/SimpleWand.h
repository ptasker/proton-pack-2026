#pragma once

#include <Arduino.h>
#include <Bounce2.h>
#include "PackController.h"
#include "WandController.h"
#include "WandMute.h"
#include "WandPackLink.h"
#include "SimpleConfig.h"

// The simple pack's wand, run on the pack ESP32 itself: the Activate switch and
// fire button feed the main wand's WandController (startup, fresh-press guard,
// 8 s firing limit, cooldown, speed levels). Vent and arm follow Activate.
// The resulting status flags drive the pack exactly as the main pack's UART
// receiver (PackWandReceiver::apply) does, and feed the pack's sound logic.
class SimpleWand
{
public:
    void begin(uint32_t now)
    {
        activateSwitch.attach(simple_config::ACTIVATE_PIN, INPUT_PULLUP);
        activateSwitch.interval(simple_config::DEBOUNCE_MS);
        fireButton.attach(simple_config::FIRE_PIN, INPUT_PULLUP);
        fireButton.interval(simple_config::DEBOUNCE_MS);
        wand.begin(now);
        mute.begin();
        readControls();
        wand.update(controls, now);
    }

    void update(PackController &pack, uint32_t now)
    {
        activateSwitch.update();
        fireButton.update();
        readControls();
        wand.update(controls, now);
        mute.update(controls, now);

        uint8_t flags = 0;
        if (controls.activate) flags |= wand_link::Activate;
        if (controls.vent) flags |= wand_link::Vent;
        if (controls.wand) flags |= wand_link::Enabled;
        if (controls.fire) flags |= wand_link::FireHeld;
        if (wand.state() == WandState::Starting) flags |= wand_link::Starting;
        if (wand.state() == WandState::Firing) flags |= wand_link::Firing;
        if (wand.cooling()) flags |= wand_link::Cooling;
        if (mute.muted()) flags |= wand_link::Muted;
        currentFlags = flags;

        apply(pack, flags, now);
        currentSpeedLevel = pack.state() == PackState::Firing ? wand.firingSpeedLevel(now) : 1;

        if (wand.state() != reportedState)
        {
            reportedState = wand.state();
            Serial.printf("Wand state: %s\n", wand.stateName());
        }
    }

    // Flags for the pack sound logic. Arm beeps and the modern themes' wand-ready
    // whine are optional (both are driven by switches the simple wand doesn't have).
    uint8_t soundFlags(SystemTheme theme) const
    {
        uint8_t flags = currentFlags;
        if (!simple_config::ARM_BEEPS)
            flags &= ~wand_link::Enabled;
        if (!simple_config::WAND_READY_HUM && config::themeIsModern(theme))
            flags &= ~wand_link::Vent;
        return flags;
    }

    // Wand switch/state flags as read (web page status).
    uint8_t flags() const { return currentFlags; }

    uint8_t speedLevel() const { return currentSpeedLevel; }
    bool muted() const { return mute.muted(); }
    const WandController &controller() const { return wand; }
    const WandControls &inputs() const { return controls; }

    void printStatus() const
    {
        Serial.printf("Wand: Activate GPIO %d, fire GPIO %d (to GND); Vent/arm follow Activate\n",
                      simple_config::ACTIVATE_PIN, simple_config::FIRE_PIN);
        mute.printStatus();
    }

private:
    Bounce activateSwitch;
    Bounce fireButton;
    WandController wand;
    WandMute mute;
    WandControls controls;
    WandState reportedState = WandState::Off;
    uint8_t currentFlags = 0;
    uint8_t currentSpeedLevel = 1;
    bool armed = false;
    bool previousFiring = false;
    bool coolingHandled = false;

    void readControls()
    {
        controls.activate = activateSwitch.read() == LOW;
        controls.vent = controls.activate;
        controls.wand = controls.activate;
        controls.fire = fireButton.read() == LOW;
    }

    // Same rules as PackWandReceiver::apply() in the main pack.
    void apply(PackController &pack, uint8_t flags, uint32_t now)
    {
        const bool firing = (flags & wand_link::Firing) != 0;
        const bool cooling = (flags & wand_link::Cooling) != 0;
        constexpr uint8_t gates = wand_link::Activate | wand_link::Vent | wand_link::Enabled;
        const bool ready = (flags & gates) == gates &&
                           (flags & (wand_link::Starting | wand_link::Cooling)) == 0;
        if (!firing)
            pack.setFiring(false, now);
        pack.setPower((flags & wand_link::Activate) != 0, now);

        if (!cooling)
            coolingHandled = false;
        else if (!coolingHandled && (pack.state() == PackState::Idle ||
                                      pack.state() == PackState::Firing ||
                                      pack.state() == PackState::Cooling))
        {
            if (pack.state() != PackState::Cooling)
                pack.vent(now);
            coolingHandled = true;
        }

        if (!ready || pack.state() != PackState::Idle)
            armed = false;
        if (firing && !previousFiring)
        {
            if (armed)
                pack.setFiring(true, now);
            else
                Serial.println("Firing ignored: release fire after startup/cooldown, then press again");
            armed = false;
        }
        else if (!firing)
        {
            armed = ready && pack.state() == PackState::Idle && (flags & wand_link::FireHeld) == 0;
        }
        previousFiring = firing;
    }
};
