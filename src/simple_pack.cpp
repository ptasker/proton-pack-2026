// Simple pack firmware (simple_pack env): one ESP32-WROOM, no wand board.
// Lights: 4 x 7 jewel cyclotron (GPIO22), 12-pixel power cell (GPIO23), and a
// 10-pixel wand chain (GPIO21). Inputs: Activate switch (GPIO33), fire (GPIO32).
// Sounds, themes and timing are the main pack's (PackController, PackSounds,
// PackAudio, PackSdAudio, WandController), included unchanged. Wi-Fi page at
// 192.168.5.1 (theme, mute, volume, music) and OTA; no Bluetooth.
// USB serial: 1-4 theme, t tone, m stop, #NNN play, d list, +/- volume.
#include <Arduino.h>
#include <Preferences.h>
#include <esp_bt.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <SPI.h>
#include "PackAudio.h"
#include "PackController.h"
#include "PackInputs.h"
#include "PackName.h"
#include "PackOta.h"
#include "PackSdAudio.h"
#include "PackSounds.h"
#include "simple/SimpleConfig.h"
#include "simple/SimpleLights.h"
#include "simple/SimpleWand.h"
#include "simple/SimpleWeb.h"

PackController pack;
PackInputs serialControls;   // USB serial keys only; the main pack's pins are all -1
PackAudio audio;
PackSdAudio sdAudio;
PackSounds sounds;
SimpleWand wand;
SimpleLights lights;
SimpleWeb web;
PackOta ota;
bool packMuted = false;

// Restart diagnostics (web page + serial): why the last restart happened, and
// how many restarts since power-on (RTC memory survives everything but power loss).
RTC_NOINIT_ATTR uint32_t restartMagic;
RTC_NOINIT_ATTR uint32_t restartsSincePowerOn;
const char *resetReason = "unknown";

const char *describeReset(esp_reset_reason_t reason)
{
    switch (reason)
    {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_BROWNOUT: return "BROWNOUT (power dip)";
    case ESP_RST_PANIC: return "CRASH (panic)";
    case ESP_RST_INT_WDT: return "CRASH (interrupt watchdog)";
    case ESP_RST_TASK_WDT: return "CRASH (task watchdog)";
    case ESP_RST_WDT: return "CRASH (watchdog)";
    case ESP_RST_SW: return "software restart";
    case ESP_RST_EXT: return "reset button";
    default: return "other";
    }
}

// After a brownout the SD card may not have lost power and can be stuck mid-read:
// with CS high, clock it until it lets go of MISO before mounting.
void releaseSdCard()
{
    SPI.begin(config::SD_SCK_PIN, config::SD_MISO_PIN, config::SD_MOSI_PIN, config::SD_CS_PIN);
    pinMode(config::SD_CS_PIN, OUTPUT);
    digitalWrite(config::SD_CS_PIN, HIGH);
    SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
    for (int i = 0; i < 600; ++i)
        SPI.transfer(0xFF);
    SPI.endTransaction();
}

void printHeap(const char *stage)
{
    Serial.printf("Heap after %s: free %u, largest block %u\n", stage,
                  static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                  static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
}

// Latest change wins, from the web page or the wand gesture.
void setPackMuted(bool muted, bool beep, const char *source)
{
    if (muted == packMuted)
        return;
    packMuted = muted;
    audio.setSdMuted(muted);
    if (beep)
        audio.requestFeedbackBeep(!muted);
    Serial.printf("Sound %s by %s\n", muted ? "MUTED" : "on", source);
}

void handleWebCommands()
{
    SimpleWeb::Command command;
    while (web.receive(command))
    {
        switch (command.type)
        {
        case SimpleWeb::CommandType::Mute:
            setPackMuted(command.value != 0, true, "web page");
            break;
        case SimpleWeb::CommandType::Volume:
            sdAudio.setVolume(command.value);
            break;
        case SimpleWeb::CommandType::Theme:
            if (command.value >= 0 && command.value <= static_cast<int32_t>(SystemTheme::FrozenEmpire))
                pack.setTheme(static_cast<SystemTheme>(command.value));
            break;
        case SimpleWeb::CommandType::PlayMusic:
        {
            if (command.value < 500 || command.value > 999)
                break;
            const uint16_t playing = sdAudio.playingMusicTrack();
            if (playing != 0)
                sdAudio.stop(playing);
            sdAudio.play(static_cast<uint16_t>(command.value), false, config::MUSIC_LEVEL_DB);
            break;
        }
        case SimpleWeb::CommandType::StopMusic:
        {
            const uint16_t playing = sdAudio.playingMusicTrack();
            if (playing != 0)
                sdAudio.stop(playing);
            break;
        }
        }
    }
}

void setup()
{
    Serial.begin(115200);
    const esp_reset_reason_t reason = esp_reset_reason();
    resetReason = describeReset(reason);
    if (reason == ESP_RST_POWERON || restartMagic != 0x51A9C0DE)
    {
        restartMagic = 0x51A9C0DE;
        restartsSincePowerOn = 0;
    }
    else
        ++restartsSincePowerOn;
    Serial.printf("\nRestart reason: %s (restarts since power-on: %lu)\n", resetReason,
                  static_cast<unsigned long>(restartsSincePowerOn));
    esp_bt_mem_release(ESP_BT_MODE_BTDM);  // no Bluetooth: give its RAM to the heap
    printHeap("boot");
    audio.attachSdSource(&sdAudio);
    audio.begin();
    const uint32_t now = millis();
    pack.begin(now, false);
    Preferences prefs;
    if (prefs.begin("pack", false)) // Read-write creates the namespace on first boot.
    {
        const uint8_t saved = prefs.getUChar("theme", static_cast<uint8_t>(config::SYSTEM_THEME));
        prefs.end();
        if (saved <= static_cast<uint8_t>(SystemTheme::FrozenEmpire))
            pack.setTheme(static_cast<SystemTheme>(saved));
    }
    serialControls.begin(pack, now);
    wand.begin(now);
    lights.begin(pack, now);
    // SD first: FATFS needs one large contiguous block before Wi-Fi fragments the heap.
    releaseSdCard();
    sdAudio.begin();
    for (int attempt = 0; attempt < 3 && !sdAudio.cardMounted(); ++attempt)
    {
        Serial.printf("SD audio: retrying mount (%d/3)\n", attempt + 1);
        delay(300);
        releaseSdCard();
        sdAudio.listTracks();   // mounts and rescans (prints the track list)
    }
    // Saved master volume (web page / USB +/-), else the simple pack default.
    {
        Preferences volumePrefs;
        int8_t volume = simple_config::DEFAULT_VOLUME_DB;
        if (volumePrefs.begin("pack", true))
        {
            volume = volumePrefs.getChar("volume", simple_config::DEFAULT_VOLUME_DB);
            volumePrefs.end();
        }
        sdAudio.setVolume(volume);
    }
    printHeap("SD audio");
    web.begin(pack_name::load());
    printHeap("Wi-Fi");
    ota.begin(web.isRunning(), []() {
        sdAudio.stopAll();
        lights.blank();
        delay(50);
    });
    printHeap("OTA");

    packMuted = wand.muted();
    audio.setSdMuted(packMuted);

    Serial.println("\nSimple Proton Pack: jewels GPIO22, power cell GPIO23, wand GPIO21");
    Serial.printf("Theme: %s (USB 1=1984, 2=Afterlife, 3=1989, 4=Frozen Empire)\n", config::themeName(pack.theme()));
    wand.printStatus();
    audio.printStatus();
    Serial.println("USB: t=tone, m=stop audio, #NNN+Enter=play track, d=list, +/-=volume, ?=status");
}

void loop()
{
    static PackState lastReportedState = pack.state();
    static SystemTheme lastReportedTheme = pack.theme();
    ota.update();
    const uint32_t now = millis();
    pack.update(now);
    serialControls.update(pack, now, audio, sdAudio);
    serialControls.takeModeToggle(); // no Bluetooth mode on the simple pack
    wand.update(pack, now);
    audio.update(now);
    sdAudio.update(now);
    sounds.update(pack, wand.soundFlags(pack.theme()), wand.speedLevel(), sdAudio, now);

    // Wand mute gesture (hold fire 3 s with Activate off, saved): each gesture
    // toggles the current mute, so it still works after a web page change.
    static bool lastWandMute = wand.muted();
    if (wand.muted() != lastWandMute)
    {
        lastWandMute = wand.muted();
        setPackMuted(!packMuted, true, "wand gesture");
    }
    handleWebCommands();

    // Duck the effects while music (500+) plays; music keeps its level.
    static bool effectsDucked = false;
    const bool musicPlaying = sdAudio.playingMusicTrack() != 0;
    if (musicPlaying != effectsDucked)
    {
        effectsDucked = musicPlaying;
        sdAudio.setEffectsTrim(musicPlaying ? config::EFFECTS_UNDER_MUSIC_DB : 0);
    }

    web.updateStatus(pack, wand, sdAudio, packMuted, resetReason, restartsSincePowerOn, now);
    lights.update(pack, wand.controller(), wand.speedLevel(), now);

    // Save the master volume once it has been unchanged for 2 s (flash wear).
    static int8_t savedVolume = sdAudio.volume();
    static int8_t pendingVolume = savedVolume;
    static uint32_t volumeChangedMs = 0;
    if (sdAudio.volume() != pendingVolume)
    {
        pendingVolume = sdAudio.volume();
        volumeChangedMs = now;
    }
    if (pendingVolume != savedVolume && now - volumeChangedMs >= 2000)
    {
        Preferences prefs;
        if (prefs.begin("pack", false))
        {
            prefs.putChar("volume", pendingVolume);
            prefs.end();
            savedVolume = pendingVolume;
            Serial.printf("Volume %d dB saved\n", static_cast<int>(savedVolume));
        }
    }

    if (lastReportedTheme != pack.theme())
    {
        lastReportedTheme = pack.theme();
        Serial.printf("Theme: %s (saved)\n", config::themeName(lastReportedTheme));
        Preferences prefs;
        if (prefs.begin("pack", false))
        {
            prefs.putUChar("theme", static_cast<uint8_t>(lastReportedTheme));
            prefs.end();
        }
    }
    if (lastReportedState != pack.state())
    {
        lastReportedState = pack.state();
        Serial.printf("Pack state: %s\n", pack.stateName());
    }
}
