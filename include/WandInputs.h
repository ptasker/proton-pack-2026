#pragma once

#include <Arduino.h>
#include <Bounce2.h>
#include "WandController.h"

class WandInputs
{
public:
    void begin()
    {
        if (wand_config::BENCH_IDLE_FIRING_MODE)
        {
            values.activate = true;
            values.vent = true;
            values.wand = true;
        }
        else
        {
            values.vent = wand_config::VENT_SWITCH_PIN < 0 &&
                          wand_config::BENCH_ENABLE_WITHOUT_VENT_SWITCH;
            values.wand = wand_config::WAND_SWITCH_PIN < 0 &&
                          wand_config::BENCH_ENABLE_WITHOUT_WAND_SWITCH;
            attach(activateSwitch, wand_config::ACTIVATE_SWITCH_PIN, values.activate);
            attach(ventSwitch, wand_config::VENT_SWITCH_PIN, values.vent);
            attach(wandSwitch, wand_config::WAND_SWITCH_PIN, values.wand);
            followWandSwitch();
        }
        attach(fireButton, wand_config::FIRE_BUTTON_PIN, values.fire);
    }

    void update()
    {
        if (!wand_config::BENCH_IDLE_FIRING_MODE)
        {
            sample(activateSwitch, wand_config::ACTIVATE_SWITCH_PIN, values.activate);
            sample(ventSwitch, wand_config::VENT_SWITCH_PIN, values.vent);
            sample(wandSwitch, wand_config::WAND_SWITCH_PIN, values.wand);
            followWandSwitch();
        }
        sample(fireButton, wand_config::FIRE_BUTTON_PIN, values.fire);

        if (Serial.available() <= 0)
            return;

        switch (Serial.read())
        {
        case 'p':
            if (!wand_config::BENCH_IDLE_FIRING_MODE && wand_config::ACTIVATE_SWITCH_PIN < 0)
                values.activate = !values.activate;
            break;
        case 'v':
            if (!wand_config::BENCH_IDLE_FIRING_MODE && wand_config::VENT_SWITCH_PIN < 0)
                values.vent = !values.vent;
            break;
        case 'w':
            if (!wand_config::BENCH_IDLE_FIRING_MODE && wand_config::WAND_SWITCH_PIN < 0)
                values.wand = !values.wand;
            break;
        case 'f':
            if (wand_config::FIRE_BUTTON_PIN < 0)
                values.fire = true;
            break;
        case 's':
            if (wand_config::FIRE_BUTTON_PIN < 0)
                values.fire = false;
            break;
        case '?':
            printHelp();
            printStatus();
            break;
        default:
            break;
        }
    }

    const WandControls &controls() const { return values; }

    void printStatus() const
    {
        Serial.printf("Inputs: activate=%u vent=%u wand=%u fire=%u\n",
                      static_cast<unsigned>(values.activate), static_cast<unsigned>(values.vent),
                      static_cast<unsigned>(values.wand), static_cast<unsigned>(values.fire));
    }

    static void printHelp()
    {
        if (wand_config::BENCH_IDLE_FIRING_MODE)
        {
            Serial.println("Idle/firing bench: switches assumed on; hold fire to fire, release to idle; ?=inputs/help");
            Serial.println("Release fire after boot. After the firing limit, wait for cooldown and make a new press.");
            return;
        }

        Serial.println("Wand serial: p=activate, v=vent gate, w=wand gate, f=fire, s=release, ?=inputs/help");
        Serial.println("Serial controls apply only to inputs whose GPIO is -1.");
    }

private:
    WandControls values;
    Bounce activateSwitch;
    Bounce ventSwitch;
    Bounce wandSwitch;
    Bounce fireButton;

    static void attach(Bounce &input, int8_t pin, bool &value)
    {
        if (pin >= 0)
        {
            input.attach(pin, INPUT_PULLUP);
            input.interval(wand_config::DEBOUNCE_MS);
            value = input.read() == LOW;
        }
    }

    void followWandSwitch()
    {
        if (wand_config::VENT_SWITCH_PIN < 0 && wand_config::VENT_FOLLOWS_WAND_SWITCH)
            values.vent = values.wand;
    }

    static void sample(Bounce &input, int8_t pin, bool &value)
    {
        if (pin >= 0)
        {
            input.update();
            value = input.read() == LOW;
        }
    }
};