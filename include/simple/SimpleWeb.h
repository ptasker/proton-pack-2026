#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "Config.h"
#include "PackController.h"
#include "PackSdAudio.h"
#include "SimpleWand.h"
#include "SimpleWebPage.h"

#if __has_include("WifiSecrets.h")
#include "WifiSecrets.h"
#define SIMPLE_WIFI_HAS_SECRETS 1
static_assert(sizeof(wifi_secrets::AP_PASSWORD) - 1 >= 8 && sizeof(wifi_secrets::AP_PASSWORD) - 1 <= 63,
              "WifiSecrets.h AP_PASSWORD must be 8-63 characters (WPA2)");
#else
#define SIMPLE_WIFI_HAS_SECRETS 0
#endif

// Simple pack Wi-Fi access point and control page (theme, mute, volume, music),
// modelled on the main pack's PackWeb. It uses 192.168.5.1 instead of the main
// pack's 192.168.4.1, so an OTA upload meant for one pack can't reach the other.
// Handlers run in the AsyncTCP task: they only queue commands and copy JSON
// snapshots published by the loop task.
class SimpleWeb
{
public:
    enum class CommandType : uint8_t { Mute, Volume, PlayMusic, StopMusic, Theme };
    struct Command
    {
        CommandType type;
        int32_t value;
    };

    static IPAddress address() { return IPAddress(192, 168, 5, 1); }

    void begin(const String &name)
    {
#if !SIMPLE_WIFI_HAS_SECRETS
        Serial.println("Wi-Fi: off; copy include/WifiSecrets.example.h to include/WifiSecrets.h and set a password");
        return;
#else
        commands = xQueueCreate(8, sizeof(Command));
        lock = xSemaphoreCreateMutex();
        if (commands == nullptr || lock == nullptr)
        {
            Serial.println("Wi-Fi: FAILED to allocate web queues");
            return;
        }
        ssid = name;
        WiFi.persistent(false);
        WiFi.mode(WIFI_AP);
        WiFi.setSleep(false);
        if (!WiFi.softAP(ssid.c_str(), wifi_secrets::AP_PASSWORD, config::WIFI_CHANNEL, false,
                         config::WIFI_MAX_CLIENTS))
        {
            Serial.println("Wi-Fi: FAILED to start access point");
            WiFi.mode(WIFI_OFF);
            return;
        }
        delay(100);
        WiFi.softAPConfig(address(), address(), IPAddress(255, 255, 255, 0));

        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(200, "text/html; charset=utf-8", SIMPLE_WEB_PAGE);
        });
        server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
            request->send(200, "application/json", snapshot(statusJson));
        });
        server.on("/api/music", HTTP_GET, [this](AsyncWebServerRequest *request) {
            request->send(200, "application/json", snapshot(musicJson));
        });
        server.on("/api/mute", HTTP_POST, [this](AsyncWebServerRequest *request) {
            queueWithParam(request, CommandType::Mute, "on");
        });
        server.on("/api/volume", HTTP_POST, [this](AsyncWebServerRequest *request) {
            queueWithParam(request, CommandType::Volume, "db");
        });
        server.on("/api/theme", HTTP_POST, [this](AsyncWebServerRequest *request) {
            queueWithParam(request, CommandType::Theme, "t");
        });
        server.on("/api/music/play", HTTP_POST, [this](AsyncWebServerRequest *request) {
            queueWithParam(request, CommandType::PlayMusic, "track");
        });
        server.on("/api/music/stop", HTTP_POST, [this](AsyncWebServerRequest *request) {
            queue(request, {CommandType::StopMusic, 0});
        });
        server.onNotFound([](AsyncWebServerRequest *request) {
            request->send(404, "text/plain", "Not found");
        });
        server.begin();
        running = true;
        printStatus();
#endif
    }

    bool isRunning() const { return running; }

    bool receive(Command &command)
    {
        return running && xQueueReceive(commands, &command, 0) == pdTRUE;
    }

    void updateStatus(const PackController &pack, const SimpleWand &wand, const PackSdAudio &sd, bool muted,
                      const char *resetReason, uint32_t restarts, uint32_t now)
    {
        if (!running)
            return;
        if (sd.indexVersion() != publishedIndex)
        {
            publishedIndex = sd.indexVersion();
            publishMusicList(sd);
        }
        if (now - lastStatusMs < STATUS_INTERVAL_MS)
            return;
        lastStatusMs = now;

        const uint8_t flags = wand.flags();
        char json[440];
        snprintf(json, sizeof(json),
                 "{\"name\":\"%s\",\"pack\":\"%s\",\"theme\":%u,\"themeName\":\"%s\",\"speed\":%u,"
                 "\"wand\":{\"activate\":%s,\"fire\":%s,\"firing\":%s,\"cooling\":%s},"
                 "\"muted\":%s,\"volume\":%d,\"music\":%u,"
                 "\"sd\":{\"mounted\":%s,\"tracks\":%u,\"index\":%lu},\"clients\":%u,\"uptime\":%lu,"
                 "\"reset\":\"%s\",\"restarts\":%lu}",
                 ssid.c_str(), pack.stateName(), static_cast<unsigned>(pack.theme()),
                 config::themeName(pack.theme()), static_cast<unsigned>(wand.speedLevel()),
                 boolText(flags & wand_link::Activate), boolText(flags & wand_link::FireHeld),
                 boolText(flags & wand_link::Firing), boolText(flags & wand_link::Cooling), boolText(muted),
                 static_cast<int>(sd.volume()), static_cast<unsigned>(sd.playingMusicTrack()),
                 boolText(sd.cardMounted()), static_cast<unsigned>(sd.trackCount()),
                 static_cast<unsigned long>(sd.indexVersion()),
                 static_cast<unsigned>(WiFi.softAPgetStationNum()), static_cast<unsigned long>(now), resetReason,
                 static_cast<unsigned long>(restarts));
        publish(statusJson, String(json));
    }

    void printStatus() const
    {
        if (!running)
            return;
        Serial.printf("Wi-Fi: network \"%s\" on channel %u; open http://%s/\n", ssid.c_str(),
                      static_cast<unsigned>(config::WIFI_CHANNEL), WiFi.softAPIP().toString().c_str());
    }

private:
    static constexpr uint32_t STATUS_INTERVAL_MS = 250;

    AsyncWebServer server{80};
    QueueHandle_t commands = nullptr;
    SemaphoreHandle_t lock = nullptr;
    String statusJson = "{}";
    String musicJson = "[]";
    String ssid;
    bool running = false;
    uint32_t lastStatusMs = 0;
    uint32_t publishedIndex = 0xFFFFFFFF;

    static const char *boolText(bool value) { return value ? "true" : "false"; }

    String snapshot(const String &source)
    {
        String copy;
        if (xSemaphoreTake(lock, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            copy = source;
            xSemaphoreGive(lock);
        }
        return copy.length() != 0 ? copy : String("{}");
    }

    void publish(String &target, const String &value)
    {
        if (xSemaphoreTake(lock, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            target = value;
            xSemaphoreGive(lock);
        }
    }

    void publishMusicList(const PackSdAudio &sd)
    {
        String json = "[";
        sd.forEachTrack([&json](uint16_t number, const char *name) {
            if (number < 500)
                return;
            if (json.length() > 1)
                json += ',';
            json += "{\"n\":";
            json += number;
            json += ",\"name\":\"";
            for (const char *p = name; *p != '\0'; ++p)
            {
                const char c = *p;
                if (c == '"' || c == '\\')
                    json += '\\';
                if (static_cast<unsigned char>(c) >= 0x20)
                    json += c;
            }
            json += "\"}";
        });
        json += ']';
        publish(musicJson, json);
    }

    void queue(AsyncWebServerRequest *request, const Command &command)
    {
        if (xQueueSend(commands, &command, 0) == pdTRUE)
            request->send(202, "application/json", "{\"ok\":true}");
        else
            request->send(503, "application/json", "{\"ok\":false,\"error\":\"busy\"}");
    }

    void queueWithParam(AsyncWebServerRequest *request, CommandType type, const char *name)
    {
        if (!request->hasParam(name))
        {
            request->send(400, "application/json", "{\"ok\":false,\"error\":\"missing parameter\"}");
            return;
        }
        queue(request, {type, static_cast<int32_t>(request->getParam(name)->value().toInt())});
    }
};
