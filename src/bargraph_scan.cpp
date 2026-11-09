#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include "WandConfig.h"

#if !defined(CONFIG_IDF_TARGET_ESP32C3)
#error "Build the bargraph scanner with the bargraph_scan_c3 environment"
#endif

constexpr int SDA_PIN = 4;
constexpr int SCL_PIN = 5;
constexpr uint8_t EXPECTED_ADDRESS = 0x70;

Adafruit_NeoPixel pixels{wand_config::PIXEL_COUNT, wand_config::PIXEL_PIN, NEO_GRB + NEO_KHZ800};
bool busReady = false;

void scanBargraphBus()
{
    Serial.println();
    Serial.println("C3 bargraph I2C scan: SDA=4, SCL=5, 100 kHz, expected address 0x70");
    if (!busReady)
    {
        Serial.println("I2C initialization failed. Reset the C3 to retry.");
        return;
    }
    if (digitalRead(SDA_PIN) == LOW || digitalRead(SCL_PIN) == LOW)
    {
        Serial.println("I2C bus held LOW. Check power, ground, SDA/SCL, and level-shifter pull-ups.");
        Serial.println("Send ? to retry after checking the wiring with power disconnected.");
        return;
    }

    uint8_t found = 0;
    bool expectedFound = false;
    for (uint8_t address = 0x08; address <= 0x77; ++address)
    {
        Wire.beginTransmission(address);
        const uint8_t error = Wire.endTransmission();
        if (error == 0)
        {
            Serial.printf("Found I2C device at 0x%02X\n", static_cast<unsigned>(address));
            ++found;
            if (address == EXPECTED_ADDRESS)
                expectedFound = true;
        }
        else if (error != 2)
        {
            Serial.printf("Scan aborted: I2C error %u at 0x%02X. Check the bus wiring.\n",
                          static_cast<unsigned>(error), static_cast<unsigned>(address));
            return;
        }
    }

    if (found == 0)
        Serial.println("No I2C devices found.");
    Serial.println(expectedFound ? "Expected device at 0x70 acknowledged; bargraph segments are not tested yet." :
                                  "Expected device at 0x70 did not respond.");
    Serial.println("Scan complete. Send ? to scan again.");
}

void setup()
{
    Serial.begin(115200);
    pixels.begin();
    pixels.clear();
    pixels.show();

    busReady = Wire.begin(SDA_PIN, SCL_PIN, 100000);
    Wire.setTimeOut(50);
    scanBargraphBus();
}

void loop()
{
    static bool monitorConnected = false;
    const bool connected = static_cast<bool>(Serial);
    if (connected && !monitorConnected)
        scanBargraphBus();
    monitorConnected = connected;

    if (Serial.available() > 0 && Serial.read() == '?')
        scanBargraphBus();
}