#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "Config.h"
#include "PackAudio.h"
#include "PackController.h"
#include "PackName.h"
#include "PackSdAudio.h"
#include "PackWandReceiver.h"
#include "PackWebPage.h"

#if __has_include("WifiSecrets.h")
#include "WifiSecrets.h"
#define PACK_WIFI_HAS_SECRETS 1
static_assert(sizeof(wifi_secrets::AP_PASSWORD) - 1 >= 8 && sizeof(wifi_secrets::AP_PASSWORD) - 1 <= 63,
              "WifiSecrets.h AP_PASSWORD must be 8-63 characters (WPA2)");
#else
#define PACK_WIFI_HAS_SECRETS 0
#endif

// Pack Wi-Fi access point and control page, after GPStar's pack web UI
// (ESPAsyncWebServer). Web handlers run in the AsyncTCP task, so they never touch
// pack objects: they queue commands for the loop task, and serve JSON snapshots
// that the loop task publishes.
class PackWeb
{
public:
    enum class CommandType : uint8_t { Mute, Volume, PlayMusic, StopMusic, Theme, BluetoothMode, PackName };
    struct Command
    {
        CommandType type;
        int32_t value;
    };

    // name: the pack name (PackName.h), used as the network name.
    void begin(const String &name)
    {
        if (!config::WIFI_ENABLED)
        {
            Serial.println("Wi-Fi: disabled in Config.h");
            return;
        }
#if !PACK_WIFI_HAS_SECRETS
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

        WiFi.persistent(false);
        WiFi.mode(WIFI_AP);
        WiFi.setSleep(false);
        ssid = name;
        if (!WiFi.softAP(ssid.c_str(), wifi_secrets::AP_PASSWORD, config::WIFI_CHANNEL, false,
                         config::WIFI_MAX_CLIENTS))
        {
            Serial.println("Wi-Fi: FAILED to start access point");
            WiFi.mode(WIFI_OFF);
            return;
        }

        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(200, "text/html; charset=utf-8", PACK_WEB_PAGE);
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
        server.on("/api/music/play", HTTP_POST, [this](AsyncWebServerRequest *request) {
            queueWithParam(request, CommandType::PlayMusic, "track");
        });
        server.on("/api/theme", HTTP_POST, [this](AsyncWebServerRequest *request) {
            queueWithParam(request, CommandType::Theme, "t");
        });
        server.on("/api/name", HTTP_POST, [this](AsyncWebServerRequest *request) {
            if (!request->hasParam("n"))
            {
                request->send(400, "application/json", "{\"ok\":false,\"error\":\"missing parameter\"}");
                return;
            }
            String name = request->getParam("n")->value();
            name.trim();
            if (name.length() != 0 && !pack_name::valid(name))
            {
                request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid name\"}");
                return;
            }
            if (xSemaphoreTake(lock, pdMS_TO_TICKS(50)) != pdTRUE)
            {
                request->send(503, "application/json", "{\"ok\":false,\"error\":\"busy\"}");
                return;
            }
            pendingName = name;
            xSemaphoreGive(lock);
            queue(request, {CommandType::PackName, 0});
        });
        server.on("/api/bluetooth", HTTP_POST, [this](AsyncWebServerRequest *request) {
            queue(request, {CommandType::BluetoothMode, 0});
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

    // Loop task: the name from the last PackName command (empty = default).
    String takePendingName()
    {
        String name;
        if (xSemaphoreTake(lock, portMAX_DELAY) == pdTRUE)
        {
            name = pendingName;
            xSemaphoreGive(lock);
        }
        return name;
    }

    // Loop task: next queued web command, if any.
    bool receive(Command &command)
    {
        return running && xQueueReceive(commands, &command, 0) == pdTRUE;
    }

    // Loop task: publish the status snapshot a few times per second.
    void updateStatus(const PackController &pack, const PackWandReceiver &wand, const PackAudio &audio,
                      const PackSdAudio &sd, bool muted, uint32_t now)
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
        char json[520];
        snprintf(json, sizeof(json),
                 "{\"name\":\"%s\",\"pack\":\"%s\",\"theme\":%u,\"themeName\":\"%s\",\"speed\":%u,"
                 "\"wand\":{\"link\":%s,\"activate\":%s,\"vent\":%s,\"arm\":%s,\"fire\":%s,"
                 "\"firing\":%s,\"cooling\":%s},"
                 "\"muted\":%s,\"volume\":%d,\"music\":%u,"
                 "\"sd\":{\"mounted\":%s,\"tracks\":%u,\"index\":%lu},"
                 "\"underruns\":%lu,\"clipped\":%lu,\"limited\":%lu,\"clients\":%u,\"uptime\":%lu}",
                 ssid.c_str(), pack.stateName(), static_cast<unsigned>(pack.theme()), config::themeName(pack.theme()),
                 static_cast<unsigned>(wand.speedLevel()),
                 boolText(wand.isConnected()), boolText(flags & wand_link::Activate),
                 boolText(flags & wand_link::Vent), boolText(flags & wand_link::Enabled),
                 boolText(flags & wand_link::FireHeld), boolText(flags & wand_link::Firing),
                 boolText(flags & wand_link::Cooling), boolText(muted), static_cast<int>(sd.volume()),
                 static_cast<unsigned>(sd.playingMusicTrack()), boolText(sd.cardMounted()),
                 static_cast<unsigned>(sd.trackCount()), static_cast<unsigned long>(sd.indexVersion()),
                 static_cast<unsigned long>(sd.underrunCount()), static_cast<unsigned long>(audio.clipCount()),
                 static_cast<unsigned long>(audio.limitedCount()),
                 static_cast<unsigned>(WiFi.softAPgetStationNum()), static_cast<unsigned long>(now));
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
    String pendingName;   // guarded by lock
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
