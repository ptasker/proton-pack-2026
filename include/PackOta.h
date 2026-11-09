#pragma once

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <functional>
#include "Config.h"
#include "PackWeb.h"

// PlatformIO firmware uploads over the pack's own Wi-Fi network (espota, UDP/TCP
// port 3232). The password is the Wi-Fi password from WifiSecrets.h; the
// pack_ota environment reads it from the same file. While an upload runs,
// ArduinoOTA.handle() blocks the loop task; the SD reader and I2S tasks keep
// running, so stop the sounds first.
class PackOta
{
public:
    void begin(bool wifiRunning, std::function<void()> onStart)
    {
        if (!config::OTA_ENABLED || !wifiRunning)
            return;
#if PACK_WIFI_HAS_SECRETS
        ArduinoOTA.setHostname("protonpack");
        ArduinoOTA.setMdnsEnabled(false);
        ArduinoOTA.setPassword(wifi_secrets::AP_PASSWORD);
        ArduinoOTA.onStart([onStart]() {
            Serial.println("OTA: firmware upload started; pack paused");
            if (onStart)
                onStart();
        });
        ArduinoOTA.onProgress([this](unsigned int done, unsigned int total) {
            const unsigned percent = total > 0 ? done * 100u / total : 0;
            if (percent >= lastPercent + 10)
            {
                lastPercent = percent - percent % 10;
                Serial.printf("OTA: %u%%\n", lastPercent);
            }
        });
        ArduinoOTA.onEnd([]() { Serial.println("OTA: upload complete; rebooting"); });
        ArduinoOTA.onError([this](ota_error_t error) {
            Serial.printf("OTA: FAILED (error %u); keeping the current firmware\n",
                          static_cast<unsigned>(error));
            lastPercent = 0;
        });
        ArduinoOTA.begin();
        running = true;
        Serial.println("OTA: ready on port 3232 (pio run -e pack_ota -t upload)");
#endif
    }

    void update()
    {
        if (running)
            ArduinoOTA.handle();
    }

private:
    bool running = false;
    unsigned lastPercent = 0;
};
