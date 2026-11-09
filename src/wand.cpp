#include <Arduino.h>
#include "WandInputs.h"
#include "WandLights.h"
#include "WandBargraph.h"
#include "WandPackSender.h"
#include "WandMute.h"

#if !defined(CONFIG_IDF_TARGET_ESP32C3)
#error "Build the wand with the wand_c3 environment"
#endif

WandController wand;
WandInputs inputs;
WandLights lights;
WandBargraph bargraph;
WandPackSender packLink;
WandMute mute;

void printWandStatus()
{
    Serial.println("ESP32-C3 Super Mini: standalone wand lights firmware");
    bargraph.printStatus();
    packLink.printStatus();
    mute.printStatus();
    if (wand_config::LED_DIAGNOSTIC_MODE)
    {
        Serial.println("LED-only diagnostic: all switches and the button are ignored");
        Serial.printf("NeoPixels: GPIO %d, count %u, brightness %u, RGB/GRB 800 kHz\n",
                      static_cast<int>(wand_config::PIXEL_PIN),
                      static_cast<unsigned>(wand_config::PIXEL_COUNT),
                      static_cast<unsigned>(wand_config::DIAGNOSTIC_BRIGHTNESS));
        Serial.println("Repeating: white pixel scan, all red, green, blue, white, off");
        return;
    }

    if (wand_config::BENCH_IDLE_FIRING_MODE)
    {
        Serial.println("Idle/firing bench: all three switches assumed on; physical fire button active");
        Serial.printf("Firing limit: %lu ms; idle lockout after limit: %lu ms\n",
                      static_cast<unsigned long>(wand_config::MAX_FIRE_MS),
                      static_cast<unsigned long>(wand_config::COOLDOWN_MS));
    }
    else
    {
        Serial.println("GPStar Super Hero switch sequence");
    }
    Serial.println("No audio, vibration, or smoke outputs");
    Serial.printf("Input GPIOs: activate=%d vent=%d wand=%d fire=%d\n",
                  static_cast<int>(wand_config::ACTIVATE_SWITCH_PIN),
                  static_cast<int>(wand_config::VENT_SWITCH_PIN),
                  static_cast<int>(wand_config::WAND_SWITCH_PIN),
                  static_cast<int>(wand_config::FIRE_BUTTON_PIN));
    if (!wand_config::BENCH_IDLE_FIRING_MODE)
        Serial.printf("Missing switch 3 bench enable: %s\n",
                      wand_config::WAND_SWITCH_PIN < 0 && wand_config::BENCH_ENABLE_WITHOUT_WAND_SWITCH ? "on" : "off");
    Serial.printf("NeoPixels: GPIO %d, count %u, brightness %u\n",
                  static_cast<int>(wand_config::PIXEL_PIN),
                  static_cast<unsigned>(wand_config::PIXEL_COUNT),
                  static_cast<unsigned>(wand_config::PIXEL_BRIGHTNESS));
    Serial.printf("LED layout: slo-blo=%u vent=%u+%u hats=%u,%u Clippard=%u firing=%u+%u\n",
                  static_cast<unsigned>(wand_config::SLO_BLO_PIXEL),
                  static_cast<unsigned>(wand_config::VENT_FIRST_PIXEL),
                  static_cast<unsigned>(wand_config::VENT_PIXELS),
                  static_cast<unsigned>(wand_config::HAT_PIXEL),
                  static_cast<unsigned>(wand_config::SECOND_HAT_PIXEL),
                  static_cast<unsigned>(wand_config::CLIPPARD_PIXEL),
                  static_cast<unsigned>(wand_config::FIRING_FIRST_PIXEL),
                  static_cast<unsigned>(wand_config::FIRING_PIXELS));
    WandInputs::printHelp();
    inputs.printStatus();
    Serial.print("Wand state: ");
    Serial.println(wand.stateName());
}

void setup()
{
    Serial.begin(115200);
    const uint32_t now = millis();
    if (!wand_config::LED_DIAGNOSTIC_MODE)
    {
        wand.begin(now);
        inputs.begin();
        wand.update(inputs.controls(), now);
        mute.begin();
    }
    lights.begin();
    bargraph.begin();
    packLink.begin(millis());
    printWandStatus();
}

void loop()
{
    static WandState lastReportedState = wand.state();
    static bool monitorConnected = false;
    const uint32_t now = millis();
    if (wand_config::LED_DIAGNOSTIC_MODE)
    {
        lights.updateDiagnostic(now);
    }
    else
    {
        inputs.update();
        wand.update(inputs.controls(), now);
        mute.update(inputs.controls(), now);
        lights.update(wand, inputs.controls(), now);
        bargraph.update(wand, packLink.theme(), now);
    }
    packLink.update(wand, inputs.controls(), mute.muted(), now);

    const bool connected = static_cast<bool>(Serial);
    if (connected && !monitorConnected)
        printWandStatus();
    monitorConnected = connected;

    if (!wand_config::LED_DIAGNOSTIC_MODE && lastReportedState != wand.state())
    {
        inputs.printStatus();
        Serial.print("Wand state: ");
        Serial.println(wand.stateName());
        lastReportedState = wand.state();
    }
}