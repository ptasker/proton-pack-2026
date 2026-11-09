#pragma once

#include "PackController.h"
#include "WandPackLink.h"

class PackWandReceiver
{
public:
    void begin()
    {
        if (config::WAND_LINK_ENABLED)
            transport.begin(config::WAND_RX_PIN, config::WAND_TX_PIN);
    }

    void update(PackController &pack, uint32_t now)
    {
        if (!config::WAND_LINK_ENABLED)
            return;

        // Expire the old session before accepting any reconnect packets.
        if (connected && now - lastReceived >= wand_link::TIMEOUT_MS)
        {
            connected = false;
            armed = false;
            previousFiring = false;
            coolingHandled = false;
            currentSpeedLevel = 1;
            currentFlags = 0;
            pack.setFiring(false, now);
            pack.setPower(false, now);
            Serial.println("Wand link lost: firing stopped, pack shutting down; fresh press required");
        }

        wand_link::Packet packet{};
        for (uint8_t handled = 0; handled < 4 && transport.receive(packet, now); ++handled)
        {
            if (packet.type != wand_link::Type::Status)
                continue;
            if (connected && packet.sequence == lastSequence)
                continue;
            if (!connected)
            {
                Serial.println("Wand link connected: waiting for ready state and released fire");
                armed = false;
            }
            connected = true;
            lastReceived = now;
            lastSequence = packet.sequence;
            currentFlags = packet.flags;
            mutedState = (packet.flags & wand_link::Muted) != 0;
            muteKnown = true;
            apply(pack, packet.flags, now);
            const uint8_t nextSpeedLevel = pack.state() == PackState::Firing ? packet.speedLevel : 1;
            if (nextSpeedLevel != currentSpeedLevel)
                Serial.printf("Wand firing speed level: %u\n", static_cast<unsigned>(nextSpeedLevel));
            currentSpeedLevel = nextSpeedLevel;
            transport.send({wand_link::Type::Ack, packet.sequence, packet.flags, packet.speedLevel}, now);
        }

        // Tell the wand the theme (for its bargraph style), on change and periodically.
        const uint8_t theme = static_cast<uint8_t>(pack.theme());
        if (connected && (theme != sentTheme || now - lastPackStatus >= wand_link::PACK_STATUS_MS))
        {
            if (transport.send({wand_link::Type::PackStatus, ++packStatusSequence, theme, 1}, now))
            {
                sentTheme = theme;
                lastPackStatus = now;
            }
        }
    }

    uint8_t speedLevel() const { return currentSpeedLevel; }

    // Latest wand switch/state flags (wand_link::Activate etc.); 0 while disconnected.
    uint8_t flags() const { return currentFlags; }

    // Wand-held sound mute. Kept after a link loss so shutdown sounds stay muted.
    bool muted() const { return mutedState; }
    bool muteStateKnown() const { return muteKnown; }
    bool isConnected() const { return connected; }

    void printStatus() const
    {
        Serial.printf("Wand link v2: %s, RX=%d (blue), TX=%d (green), 9600 8N1; %s\n",
                      config::WAND_LINK_ENABLED ? "enabled" : "disabled",
                      static_cast<int>(config::WAND_RX_PIN),
                      static_cast<int>(config::WAND_TX_PIN),
                      connected ? "connected" : "waiting for wand");
    }

private:
    wand_link::Transport transport;
    bool connected = false;
    bool armed = false;
    bool previousFiring = false;
    bool coolingHandled = false;
    uint8_t lastSequence = 0;
    uint8_t currentSpeedLevel = 1;
    uint8_t currentFlags = 0;
    uint8_t sentTheme = 0xFF;
    uint8_t packStatusSequence = 0;
    uint32_t lastPackStatus = 0;
    bool mutedState = false;
    bool muteKnown = false;
    uint32_t lastReceived = 0;

    void apply(PackController &pack, uint8_t flags, uint32_t now)
    {
        const bool firing = (flags & wand_link::Firing) != 0;
        const bool cooling = (flags & wand_link::Cooling) != 0;
        constexpr uint8_t gates = wand_link::Activate | wand_link::Vent | wand_link::Enabled;
        const bool ready = (flags & gates) == gates &&
                           (flags & (wand_link::Starting | wand_link::Cooling)) == 0;
        if (!firing)
            pack.setFiring(false, now);
        pack.setPower((flags & wand_link::Activate) != 0, now);

        if (!cooling)
            coolingHandled = false;
        else if (!coolingHandled && (pack.state() == PackState::Idle ||
                                      pack.state() == PackState::Firing ||
                                      pack.state() == PackState::Cooling))
        {
            if (pack.state() != PackState::Cooling)
                pack.vent(now);
            coolingHandled = true;
        }

        if (!ready || pack.state() != PackState::Idle)
            armed = false;
        if (firing && !previousFiring)
        {
            if (armed)
                pack.setFiring(true, now);
            else
                Serial.println("Wand firing ignored: release fire after pack startup/reconnect/cooldown");
            armed = false;
        }
        else if (!firing)
        {
            armed = ready && pack.state() == PackState::Idle &&
                    (flags & wand_link::FireHeld) == 0;
        }
        previousFiring = firing;
    }
};
