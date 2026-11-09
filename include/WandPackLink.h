#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>
#include <string.h>

namespace wand_link
{
    constexpr uint32_t BAUD = 9600;
    constexpr uint32_t HEARTBEAT_MS = 100;
    constexpr uint32_t TIMEOUT_MS = 500;
    constexpr uint32_t MIN_SEND_MS = 20;
    constexpr uint32_t FRAME_TIMEOUT_MS = 50;

    enum Flag : uint8_t
    {
        Activate = 0x01,
        Vent = 0x02,
        Enabled = 0x04,
        FireHeld = 0x08,
        Starting = 0x10,
        Firing = 0x20,
        Cooling = 0x40,
        Muted = 0x80  // Sound muted (hold fire 3 s with Activate off); both boards must know it
    };

    // PackStatus (pack -> wand) carries the pack's system theme in the flags byte.
    enum class Type : uint8_t { Status = 1, Ack = 2, PackStatus = 3 };

    // Must match SystemTheme in the pack's Config.h.
    enum Theme : uint8_t { Theme1984 = 0, Theme1989 = 1, ThemeAfterlife = 2, ThemeFrozenEmpire = 3 };
    constexpr uint32_t PACK_STATUS_MS = 500;

    struct Packet
    {
        Type type;
        uint8_t sequence;
        uint8_t flags;
        uint8_t speedLevel;
    };

    inline bool validFlags(uint8_t flags)
    {
        const uint8_t phase = flags & (Starting | Firing | Cooling);
        if (phase != 0 && (phase & (phase - 1)) != 0)
            return false;
        if (phase != 0 && (flags & Activate) == 0)
            return false;
        constexpr uint8_t firingInputs = Activate | Vent | Enabled | FireHeld;
        return (flags & Firing) == 0 || (flags & firingInputs) == firingInputs;
    }

    class Transport
    {
    public:
        void begin(int8_t rx, int8_t tx)
        {
            port.begin(BAUD, SERIAL_8N1, rx, tx);
        }

        bool send(const Packet &packet, uint32_t now)
        {
            uint8_t frame[] = {0xA5, 0x5A, 2, static_cast<uint8_t>(packet.type),
                               packet.sequence, packet.flags, packet.speedLevel, 0};
            frame[7] = checksum(frame, 7);
            if (port.availableForWrite() < static_cast<int>(sizeof(frame)))
            {
                reportError("TX buffer full", now);
                return false;
            }
            if (port.write(frame, sizeof(frame)) != sizeof(frame))
            {
                reportError("short UART write", now);
                return false;
            }
            return true;
        }

        bool receive(Packet &packet, uint32_t now)
        {
            if (used != 0 && now - lastByte >= FRAME_TIMEOUT_MS)
            {
                used = 0;
                reportError("partial frame timed out", now);
            }
            for (uint8_t read = 0; read < 32 && port.available() > 0; ++read)
            {
                const uint8_t byte = static_cast<uint8_t>(port.read());
                lastByte = now;
                if (used == 0 && byte != 0xA5)
                {
                    reportError("unexpected UART data", now);
                    continue;
                }
                frame[used++] = byte;
                if (used != sizeof(frame))
                    continue;

                const bool packStatus = frame[3] == static_cast<uint8_t>(Type::PackStatus);
                const bool statusOrAck = frame[3] == static_cast<uint8_t>(Type::Status) ||
                                         frame[3] == static_cast<uint8_t>(Type::Ack);
                const bool valid = frame[0] == 0xA5 && frame[1] == 0x5A && frame[2] == 2 &&
                                   ((statusOrAck && validFlags(frame[5]) && frame[6] >= 1 && frame[6] <= 4 &&
                                     ((frame[5] & Firing) != 0 || frame[6] == 1)) ||
                                    (packStatus && frame[5] <= ThemeFrozenEmpire && frame[6] == 1)) &&
                                   frame[7] == checksum(frame, 7);
                if (valid)
                {
                    packet = {static_cast<Type>(frame[3]), frame[4], frame[5], frame[6]};
                    used = 0;
                    return true;
                }
                reportError("invalid frame/version/CRC", now);
                memmove(frame, frame + 1, sizeof(frame) - 1);
                used = sizeof(frame) - 1;
            }
            return false;
        }

    private:
        HardwareSerial port{1};
        uint8_t frame[8] = {};
        uint8_t used = 0;
        uint32_t lastByte = 0;
        uint32_t errors = 0;
        uint32_t lastErrorReport = 0;

        static uint8_t checksum(const uint8_t *data, uint8_t size)
        {
            uint8_t crc = 0;
            for (uint8_t i = 0; i < size; ++i)
            {
                crc ^= data[i];
                for (uint8_t bit = 0; bit < 8; ++bit)
                    crc = (crc & 0x80) != 0 ? (crc << 1) ^ 0x07 : crc << 1;
            }
            return crc;
        }

        void reportError(const char *message, uint32_t now)
        {
            ++errors;
            if (errors == 1 || now - lastErrorReport >= 1000)
            {
                Serial.printf("Wand/pack UART: %s (errors=%lu)\n", message,
                              static_cast<unsigned long>(errors));
                lastErrorReport = now;
            }
        }
    };
}
