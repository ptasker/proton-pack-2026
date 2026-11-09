#pragma once

#include <Preferences.h>
#include "WandController.h"
#include "WandPackLink.h"

class WandPackSender
{
public:
    void begin(uint32_t now)
    {
        if (!wand_config::PACK_LINK_ENABLED)
            return;
        transport.begin(wand_config::PACK_RX_PIN, wand_config::PACK_TX_PIN);
        lastAck = now;
        // Last theme reported by the pack, so the bargraph style is right at power-up.
        Preferences prefs;
        if (prefs.begin("wand", false))
        {
            const uint8_t saved = prefs.getUChar("theme", wand_link::ThemeAfterlife);
            prefs.end();
            if (saved <= wand_link::ThemeFrozenEmpire)
                currentTheme = saved;
        }
    }

    void update(const WandController &wand, const WandControls &controls, bool muted, uint32_t now)
    {
        if (!wand_config::PACK_LINK_ENABLED)
            return;

        wand_link::Packet packet{};
        for (uint8_t handled = 0; handled < 4 && transport.receive(packet, now); ++handled)
        {
            if (packet.type == wand_link::Type::PackStatus)
            {
                if (packet.flags != currentTheme)
                {
                    currentTheme = packet.flags;
                    Preferences prefs;
                    if (prefs.begin("wand", false))
                    {
                        prefs.putUChar("theme", currentTheme);
                        prefs.end();
                    }
                    Serial.printf("Pack theme: %s\n", themeName());
                }
                continue;
            }
            if (sent && packet.type == wand_link::Type::Ack &&
                packet.sequence == sequence && packet.flags == lastFlags &&
                packet.speedLevel == lastSpeedLevel)
            {
                lastAck = now;
                if (!connected)
                    Serial.println("Pack link: acknowledgements received");
                connected = true;
                timeoutReported = false;
            }
        }
        if (now - lastAck >= wand_link::TIMEOUT_MS && !timeoutReported)
        {
            Serial.println("Pack link: no acknowledgements; check pack firmware and green return wire");
            connected = false;
            timeoutReported = true;
        }

        uint8_t flags = 0;
        if (!wand_config::LED_DIAGNOSTIC_MODE)
        {
            if (controls.activate) flags |= wand_link::Activate;
            if (controls.vent) flags |= wand_link::Vent;
            if (controls.wand) flags |= wand_link::Enabled;
            if (controls.fire) flags |= wand_link::FireHeld;
            if (wand.state() == WandState::Starting) flags |= wand_link::Starting;
            if (wand.state() == WandState::Firing) flags |= wand_link::Firing;
            if (wand.cooling()) flags |= wand_link::Cooling;
        }
        if (muted) flags |= wand_link::Muted;
        const uint8_t speedLevel = (flags & wand_link::Firing) != 0 ? wand.firingSpeedLevel(now) : 1;
        if (sent && (now - lastSent < wand_link::MIN_SEND_MS ||
                     (flags == lastFlags && speedLevel == lastSpeedLevel &&
                      now - lastSent < wand_link::HEARTBEAT_MS)))
            return;

        const uint8_t nextSequence = sequence + 1;
        if (transport.send({wand_link::Type::Status, nextSequence, flags, speedLevel}, now))
        {
            sequence = nextSequence;
            lastFlags = flags;
            lastSpeedLevel = speedLevel;
            lastSent = now;
            sent = true;
        }
    }

    // System theme from the pack (wand_link::Theme), saved across power-off.
    uint8_t theme() const { return currentTheme; }

    const char *themeName() const
    {
        return currentTheme == wand_link::Theme1984 ? "1984" : currentTheme == wand_link::Theme1989 ? "1989" :
               currentTheme == wand_link::ThemeFrozenEmpire ? "Frozen Empire" : "Afterlife";
    }

    void printStatus() const
    {
        Serial.printf("Pack link v2: %s, RX=%d (green), TX=%d (blue), 9600 8N1; %s\n",
                      wand_config::PACK_LINK_ENABLED ? "enabled" : "disabled",
                      static_cast<int>(wand_config::PACK_RX_PIN),
                      static_cast<int>(wand_config::PACK_TX_PIN),
                      connected ? "pack acknowledged" : "no current acknowledgement");
    }

private:
    wand_link::Transport transport;
    bool sent = false;
    bool connected = false;
    bool timeoutReported = false;
    uint8_t sequence = 0;
    uint8_t lastFlags = 0;
    uint8_t lastSpeedLevel = 1;
    uint8_t currentTheme = wand_link::ThemeAfterlife;
    uint32_t lastSent = 0;
    uint32_t lastAck = 0;
};
