#include <Arduino.h>
#include <Preferences.h>
#include <esp_bt.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include "PackInputs.h"
#include "PackLights.h"
#include "PackSounds.h"
#include "PackWandReceiver.h"
#include "PackOta.h"
#include "PackName.h"
#include "PackWeb.h"

PackController pack;
PackInputs inputs;
PackLights lights;
PackWandReceiver wandLink;
PackAudio audio;
PackSdAudio sdAudio;
PackSounds sounds;
PackWeb web;
PackOta ota;
PackBluetooth bluetooth;

// Bluetooth speaker mode survives only a software restart (esp_restart); any
// power-on, upload, or crash reset boots back into Wi-Fi mode.
constexpr uint32_t BLUETOOTH_MODE_MAGIC = 0xB1E700D5;
RTC_NOINIT_ATTR uint32_t bootModeMagic;
bool bluetoothMode = false;
uint32_t restartAtMs = 0;     // nonzero: restart at this time
bool restartToBluetooth = false;

// Keep the Bluetooth controller's RAM reserved at boot (Arduino releases it
// otherwise); Wi-Fi mode hands it back to the heap at the start of setup().
extern "C" bool btInUse() { return true; }

// Beep, then restart (after the beep) in Wi-Fi or Bluetooth speaker mode.
void scheduleRestart(bool toBluetooth)
{
    if (restartAtMs != 0)
        return;
    sdAudio.stopAll();
    audio.requestFeedbackBeep(toBluetooth || !bluetoothMode);
    restartToBluetooth = toBluetooth;
    restartAtMs = millis() + 400;
}

void requestModeSwitch(const char *source)
{
    if (!config::BLUETOOTH_ENABLED || restartAtMs != 0)
        return;
    Serial.printf("Mode: switching to %s by %s; restarting\n",
                  bluetoothMode ? "Wi-Fi" : "Bluetooth speaker", source);
    scheduleRestart(!bluetoothMode);
}

// Pack sound mute: the latest change wins, from the wand gesture or the web page.
bool packMuted = false;
bool muteInitialized = false;

void printHeap(const char *stage)
{
    Serial.printf("Heap after %s: free %u, largest block %u\n", stage,
                  static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                  static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
}

void setPackMuted(bool muted, bool beep, const char *source)
{
    if (muteInitialized && muted == packMuted)
        return;
    audio.setSdMuted(muted);
    if (beep && muteInitialized)
        audio.requestFeedbackBeep(!muted);
    Serial.printf("Sound %s by %s\n", muted ? "MUTED" : "on", source);
    packMuted = muted;
    muteInitialized = true;
}

void handleWebCommands()
{
    PackWeb::Command command;
    while (web.receive(command))
    {
        switch (command.type)
        {
        case PackWeb::CommandType::Mute:
            setPackMuted(command.value != 0, true, "web page");
            break;
        case PackWeb::CommandType::Volume:
            sdAudio.setVolume(command.value);
            break;
        case PackWeb::CommandType::PlayMusic:
        {
            if (command.value < 500 || command.value > 999)
                break;
            const uint16_t playing = sdAudio.playingMusicTrack();
            if (playing != 0)
                sdAudio.stop(playing);
            sdAudio.play(static_cast<uint16_t>(command.value), false, config::MUSIC_LEVEL_DB);
            break;
        }
        case PackWeb::CommandType::BluetoothMode:
            requestModeSwitch("web page");
            break;
        case PackWeb::CommandType::PackName:
        {
            const String name = web.takePendingName();
            if (pack_name::save(name))
            {
                Serial.printf("Pack name: \"%s\" saved; restarting\n",
                              name.length() ? name.c_str() : pack_name::defaultName().c_str());
                scheduleRestart(false);
            }
            else
                Serial.println("Pack name: could not save to flash");
            break;
        }
        case PackWeb::CommandType::Theme:
            if (command.value >= 0 && command.value <= static_cast<int32_t>(SystemTheme::FrozenEmpire))
                pack.setTheme(static_cast<SystemTheme>(command.value));
            break;
        case PackWeb::CommandType::StopMusic:
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
    bluetoothMode = config::BLUETOOTH_ENABLED && esp_reset_reason() == ESP_RST_SW &&
                    bootModeMagic == BLUETOOTH_MODE_MAGIC;
    bootModeMagic = 0;
    if (!bluetoothMode)
        esp_bt_mem_release(ESP_BT_MODE_BTDM);  // controller + Bluedroid RAM back to the heap
    else
        esp_bt_controller_mem_release(ESP_BT_MODE_BLE);  // classic only
    printHeap("boot");
    audio.attachSdSource(&sdAudio);
    audio.begin();
    const uint32_t now = millis();
    pack.begin(now, !config::WAND_LINK_ENABLED &&
                   config::PACK_SWITCH_PIN < 0 && config::AUTO_START_WITHOUT_SWITCH);
    // Restore the saved GPStar system theme.
    Preferences prefs;
    if (prefs.begin("pack", false)) // Read-write creates the namespace on first boot.
    {
        const uint8_t saved = prefs.getUChar("theme", static_cast<uint8_t>(config::SYSTEM_THEME));
        prefs.end();
        if (saved <= static_cast<uint8_t>(SystemTheme::FrozenEmpire))
            pack.setTheme(static_cast<SystemTheme>(saved));
    }
    inputs.begin(pack, now);
    wandLink.begin();
    lights.begin(pack, now);

    Serial.println("Proton Pack: lights, I2S audio and GPStar Afterlife SD sounds; no smoke");
    Serial.printf("Theme: %s\n", config::themeName(pack.theme()));
    Serial.printf("Ring: %u pixels, GPIO %u\n",
                  static_cast<unsigned>(config::RING_PIXELS),
                  static_cast<unsigned>(config::RING_PIN));
    Serial.printf("Power cell: %u pixels, GPIO %d, brightness %u\n",
                  static_cast<unsigned>(config::POWER_BAR_PIXELS),
                  static_cast<int>(config::POWER_BAR_PIN),
                  static_cast<unsigned>(config::POWER_BAR_BRIGHTNESS));
    PackInputs::printHelp();
    wandLink.printStatus();
    audio.printStatus();
    printHeap("lights and audio");
    // Bluetooth mode has no pack sounds: the SD mixer's ~80 KB is needed for pairing.
    if (bluetoothMode)
        Serial.println("SD audio: off in Bluetooth speaker mode");
    else
        sdAudio.begin();
    printHeap("SD audio");
    const String packName = pack_name::load();
    if (bluetoothMode)
    {
        if (bluetooth.begin(packName))
            audio.attachBluetoothSource(&bluetooth);
        printHeap("Bluetooth");
    }
    else
    {
        web.begin(packName);
        printHeap("Wi-Fi");
        ota.begin(web.isRunning(), []() {
            sdAudio.stopAll();
            lights.blank();
            delay(50); // let the SD reader apply the 5 ms stop before flash writes begin
        });
        printHeap("OTA");
    }
    Serial.print("Pack state: ");
    Serial.println(pack.stateName());
}

void loop()
{
    static PackState lastReportedState = pack.state();
    static SystemTheme lastReportedTheme = pack.theme();
    ota.update();
    const uint32_t now = millis();
    pack.update(now);
    inputs.update(pack, now, audio, sdAudio);
    audio.update(now);
    sdAudio.update(now);
    wandLink.update(pack, now);
    sounds.update(pack, wandLink.flags(), wandLink.speedLevel(), sdAudio, now);

    // Wand mute: its saved state applies silently at connect; each later gesture
    // toggles the pack mute, so it still works after a web page change.
    static bool wandMuteSeen = false;
    static bool lastWandMute = false;
    if (wandLink.muteStateKnown())
    {
        if (!wandMuteSeen)
        {
            wandMuteSeen = true;
            lastWandMute = wandLink.muted();
            setPackMuted(lastWandMute, false, "wand (saved setting)");
        }
        else if (wandLink.muted() != lastWandMute)
        {
            lastWandMute = wandLink.muted();
            setPackMuted(!packMuted, true, "wand");
        }
    }
    handleWebCommands();
    bluetooth.update(now);

    // Mode gesture: with Activate off, Vent on three times within MODE_GESTURE_MS.
    static uint8_t ventPresses = 0;
    static uint32_t firstVentMs = 0;
    static bool lastVent = false;
    const uint8_t wandFlags = wandLink.isConnected() ? wandLink.flags() : 0;
    const bool vent = (wandFlags & wand_link::Vent) != 0;
    if ((wandFlags & wand_link::Activate) != 0)
        ventPresses = 0;
    else if (vent && !lastVent)
    {
        if (ventPresses == 0 || now - firstVentMs > config::MODE_GESTURE_MS)
        {
            ventPresses = 0;
            firstVentMs = now;
        }
        ++ventPresses;
        Serial.printf("Mode gesture: Vent %u/3\n", static_cast<unsigned>(ventPresses));
        if (ventPresses >= 3)
        {
            ventPresses = 0;
            requestModeSwitch("wand Vent gesture");
        }
    }
    lastVent = vent;
    if (inputs.takeModeToggle())
        requestModeSwitch("USB b");
    if (restartAtMs != 0 && static_cast<int32_t>(now - restartAtMs) >= 0)
    {
        bootModeMagic = restartToBluetooth ? BLUETOOTH_MODE_MAGIC : 0;
        Serial.flush();
        esp_restart();
    }

    // Duck the pack and wand effects while music plays; music keeps its level.
    static bool effectsDucked = false;
    const bool musicPlaying = sdAudio.playingMusicTrack() != 0 || bluetooth.playing(now);
    if (musicPlaying != effectsDucked)
    {
        effectsDucked = musicPlaying;
        sdAudio.setEffectsTrim(musicPlaying ? config::EFFECTS_UNDER_MUSIC_DB : 0);
    }
    web.updateStatus(pack, wandLink, audio, sdAudio, packMuted, now);
    lights.update(pack, now, wandLink.speedLevel());

    // Theme changes come from USB keys 1-4 or the web page; save each one.
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
        Serial.print("Pack state: ");
        Serial.println(pack.stateName());
        lastReportedState = pack.state();
    }
}