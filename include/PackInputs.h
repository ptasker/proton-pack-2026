#pragma once

#include <Arduino.h>
#include <Bounce2.h>
#include "PackController.h"
#include "PackAudio.h"

class PackInputs
{
public:
    void begin(PackController &pack, uint32_t now)
    {
        attach(packSwitch, config::PACK_SWITCH_PIN);
        attach(fireButton, config::FIRE_BUTTON_PIN);
        attach(ventButton, config::VENT_BUTTON_PIN);

        if (config::PACK_SWITCH_PIN >= 0 && !config::PACK_SWITCH_MOMENTARY)
            pack.setPower(packSwitch.read() == LOW, now);
    }

    void update(PackController &pack, uint32_t now, PackAudio &audio, PackSdAudio &sdAudio)
    {
        if (config::PACK_SWITCH_PIN >= 0)
        {
            packSwitch.update();
            if (config::PACK_SWITCH_MOMENTARY)
            {
                if (packSwitch.fell())
                    pack.togglePower(now);
            }
            else if (packSwitch.changed())
            {
                pack.setPower(packSwitch.read() == LOW, now);
            }
        }

        if (config::FIRE_BUTTON_PIN >= 0)
        {
            fireButton.update();
            if (fireButton.fell() || fireButton.rose())
                pack.setFiring(fireButton.read() == LOW, now);
        }

        if (config::VENT_BUTTON_PIN >= 0)
        {
            ventButton.update();
            if (ventButton.fell())
                pack.vent(now);
        }

        for (uint8_t handled = 0; handled < 16 && Serial.available() > 0; ++handled)
        {
            const int received = Serial.read();
            if (trackEntry)
            {
                if (received >= '0' && received <= '9' && trackDigits < 3)
                {
                    trackNumber = static_cast<uint16_t>(trackNumber * 10 + (received - '0'));
                    ++trackDigits;
                    continue;
                }
                trackEntry = false;
                if ((received == '\r' || received == '\n') && trackNumber > 0)
                    sdAudio.play(trackNumber);
                else
                    Serial.println("Track entry cancelled; type # then 1-3 digits then Enter, e.g. #15");
                continue;
            }

            switch (received)
            {
            case '1':
                pack.setTheme(SystemTheme::Gb1984);
                break;
            case '2':
                pack.setTheme(SystemTheme::Afterlife);
                break;
            case '3':
                pack.setTheme(SystemTheme::Gb1989);
                break;
            case '4':
                pack.setTheme(SystemTheme::FrozenEmpire);
                break;
            case 'p':
                if (config::WAND_LINK_ENABLED)
                    Serial.println("Power command ignored: controlled by the C3 wand");
                else if (config::PACK_SWITCH_PIN < 0)
                    pack.togglePower(now);
                break;
            case 'f':
                if (config::WAND_LINK_ENABLED)
                    Serial.println("Fire command ignored: controlled by the C3 wand");
                else if (config::FIRE_BUTTON_PIN < 0)
                    pack.setFiring(true, now);
                break;
            case 's':
                if (config::WAND_LINK_ENABLED)
                    Serial.println("Release command ignored: controlled by the C3 wand");
                else if (config::FIRE_BUTTON_PIN < 0)
                    pack.setFiring(false, now);
                break;
            case 'v':
                if (config::WAND_LINK_ENABLED)
                    Serial.println("Vent command ignored: controlled by the C3 wand");
                else if (config::VENT_BUTTON_PIN < 0)
                    pack.vent(now);
                break;
            case 't':
                audio.requestTone();
                break;
            case 'm':
                audio.silence();
                sdAudio.stopAll();
                break;
            case '#':
                trackEntry = true;
                trackNumber = 0;
                trackDigits = 0;
                break;
            case 'd':
                sdAudio.listTracks();
                break;
            case '+':
                sdAudio.changeVolume(3);
                break;
            case '-':
                sdAudio.changeVolume(-3);
                break;
            case 'b':
                modeToggleRequested = true;
                break;
            case '?':
                printHelp();
                audio.printStatus();
                sdAudio.printStatus();
                break;
            default:
                break;
            }
        }
    }

    static void printHelp()
    {
        Serial.println("Serial: p=power, f=fire press, s=fire release, v=vent, ?=help");
        Serial.println("Theme: 1=1984, 2=Afterlife, 3=1989, 4=Frozen Empire (saved; also on the web page)");
        Serial.println("Audio: t=1-second test tone, m=stop all audio, b=switch Wi-Fi/Bluetooth speaker mode");
        Serial.println("SD audio: #NNN then Enter=play track (e.g. #15), d=list tracks, +/-=volume 3 dB");
        Serial.println(config::WAND_LINK_ENABLED ?
                       "C3 controls power/fire/cooldown; serial p/f/s/v are disabled." :
                       "Power/fire/vent serial controls apply only to inputs whose pin is -1.");
    }

    // USB b: switch between Wi-Fi and Bluetooth speaker mode (restarts the pack).
    bool takeModeToggle()
    {
        const bool requested = modeToggleRequested;
        modeToggleRequested = false;
        return requested;
    }

private:
    bool modeToggleRequested = false;
    Bounce packSwitch;
    Bounce fireButton;
    Bounce ventButton;
    bool trackEntry = false;
    uint16_t trackNumber = 0;
    uint8_t trackDigits = 0;

    static void attach(Bounce &input, int8_t pin)
    {
        if (pin >= 0)
        {
            input.attach(pin, INPUT_PULLUP);
            input.interval(config::DEBOUNCE_MS);
        }
    }
};
