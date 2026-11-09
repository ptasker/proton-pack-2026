#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <esp_mac.h>

// One pack name for the Wi-Fi network and the Bluetooth speaker, saved on each
// pack (Preferences namespace "pack", key "name") and set from the web page.
// Without a saved name: ProtonPack_XXXX from the Wi-Fi MAC.
namespace pack_name
{
    constexpr size_t MAX_LENGTH = 24;

    // 1-24 characters: letters, digits, space, '-', '_', '.'; no outer spaces.
    inline bool valid(const String &name)
    {
        if (name.length() == 0 || name.length() > MAX_LENGTH || name[0] == ' ' ||
            name[name.length() - 1] == ' ')
            return false;
        for (size_t i = 0; i < name.length(); ++i)
        {
            const char c = name[i];
            if (!isalnum(static_cast<unsigned char>(c)) && c != ' ' && c != '-' && c != '_' && c != '.')
                return false;
        }
        return true;
    }

    inline String defaultName()
    {
        uint8_t mac[6] = {};
        esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
        char name[20];
        snprintf(name, sizeof(name), "ProtonPack_%02X%02X", mac[4], mac[5]);
        return String(name);
    }

    inline String load()
    {
        Preferences prefs;
        String name;
        if (prefs.begin("pack", true))
        {
            name = prefs.getString("name", "");
            prefs.end();
        }
        return valid(name) ? name : defaultName();
    }

    // Empty name: back to the default.
    inline bool save(const String &name)
    {
        Preferences prefs;
        if (!prefs.begin("pack", false))
            return false;
        const bool ok = name.length() == 0 ? (prefs.remove("name"), true) : prefs.putString("name", name) > 0;
        prefs.end();
        return ok;
    }
}
