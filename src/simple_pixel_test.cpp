// Simple pack NeoPixel test (simple_pixel_test). For each chain in turn: a white
// dot steps along every pixel, then the chain goes red, green, blue, white and
// off, then each group shows its own colour. Other chains stay dark meanwhile.
// At boot, each chain's GPIO blinks slowly three times to identify its terminal.
// USB serial reports each step. Not used by the pack or wand builds.
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

namespace
{
    struct Chain
    {
        const char *name;
        int8_t pin;
        uint16_t count;
        const uint8_t *groups;   // pixels per group, in chain order (colours, labels)
        uint8_t groupCount;
        bool enabled;            // false: skip (e.g. chain unplugged)
        Adafruit_NeoPixel pixels;
    };

    constexpr uint8_t JEWEL_GROUPS[] = {7, 7, 7, 7};
    constexpr uint8_t POWER_CELL_GROUPS[] = {4, 4, 4};
    constexpr uint8_t WAND_GROUPS[] = {4, 3, 3};   // 4-pixel strip, 3 singles, 3-pixel firing strip

    // Simple pack wiring (planned pins; jewels and wand pins verified).
    Chain chains[] = {
        {"Cyclotron jewels (4 x 7)", 22, 28, JEWEL_GROUPS, 4, true,
         Adafruit_NeoPixel(28, 22, NEO_GRB + NEO_KHZ800)},
        {"Power cell (12-pixel strip)", 23, 12, POWER_CELL_GROUPS, 3, true,
         Adafruit_NeoPixel(12, 23, NEO_GRB + NEO_KHZ800)},
        {"Wand chain (4-strip + 3 singles + 3 firing)", 21, 10, WAND_GROUPS, 3, true,
         Adafruit_NeoPixel(10, 21, NEO_GRB + NEO_KHZ800)},
    };

    // Group index (0-based) and position within it for pixel i.
    void locate(const Chain &chain, uint16_t i, uint8_t &group, uint8_t &position)
    {
        group = 0;
        while (group + 1 < chain.groupCount && i >= chain.groups[group])
            i -= chain.groups[group++];
        position = static_cast<uint8_t>(i);
    }

    constexpr uint8_t BRIGHTNESS = 32;        // of 255; keeps current low

    // Input test (INPUT_TEST = true replaces the light cycle): Activate on GPIO33
    // lights the cyclotron green; fire on GPIO32 lights the wand firing strip
    // white and the power cell blue (as wired, user-verified 2026-10-17). Switch/button to GND, internal pull-ups.
    // Any other free candidate pin that changes is logged (wiring finder).
    constexpr bool INPUT_TEST = true;
    constexpr int8_t ACTIVATE_PIN = 33;
    constexpr int8_t FIRE_PIN = 32;
    constexpr int8_t CANDIDATE_PINS[] = {32, 33, 25, 16, 17, 26, 27, 14, 13, 4, 18, 19};
    constexpr size_t CANDIDATES = sizeof(CANDIDATE_PINS) / sizeof(CANDIDATE_PINS[0]);
    bool reported[CANDIDATES] = {};
    bool previous[CANDIDATES] = {};
    constexpr uint32_t SCAN_STEP_MS = 300;
    constexpr uint32_t COLOUR_MS = 1000;

    void fill(Chain &chain, uint32_t colour, const char *name)
    {
        Serial.printf("  all %u pixels: %s\n", static_cast<unsigned>(chain.count), name);
        chain.pixels.fill(colour);
        chain.pixels.show();
        delay(COLOUR_MS);
    }

    void test(Chain &chain)
    {
        Adafruit_NeoPixel &pixels = chain.pixels;
        Serial.printf("%s on GPIO %d: white dot from the first pixel (DIN end)\n", chain.name, chain.pin);
        for (uint16_t i = 0; i < chain.count; ++i)
        {
            uint8_t group = 0, position = 0;
            locate(chain, i, group, position);
            Serial.printf("  pixel %u (group %u, position %u)\n", static_cast<unsigned>(i),
                          static_cast<unsigned>(group + 1), static_cast<unsigned>(position + 1));
            pixels.clear();
            pixels.setPixelColor(i, pixels.Color(255, 255, 255));
            pixels.show();
            delay(SCAN_STEP_MS);
        }

        fill(chain, pixels.Color(255, 0, 0), "red");
        fill(chain, pixels.Color(0, 255, 0), "green");
        fill(chain, pixels.Color(0, 0, 255), "blue");
        fill(chain, pixels.Color(255, 255, 255), "white");
        fill(chain, 0, "off");

        Serial.println("  groups: 1 red, 2 green, 3 blue, 4 yellow");
        const uint32_t groupColours[] = {pixels.Color(255, 0, 0), pixels.Color(0, 255, 0),
                                         pixels.Color(0, 0, 255), pixels.Color(255, 160, 0)};
        pixels.clear();
        for (uint16_t i = 0; i < chain.count; ++i)
        {
            uint8_t group = 0, position = 0;
            locate(chain, i, group, position);
            pixels.setPixelColor(i, groupColours[group % 4]);
        }
        pixels.show();
        delay(COLOUR_MS * 2);
        pixels.clear();
        pixels.show();
        delay(500);
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println("\nSimple pack pixel test (GRB, 800 kHz, brightness 32/255)");
    // Pin finder: slow blinks show which breakout-board LED/terminal is each data pin.
    for (Chain &chain : chains)
    {
        if (!chain.enabled)
            continue;
        pinMode(chain.pin, OUTPUT);
        for (int i = 0; i < 3; ++i)
        {
            Serial.printf("GPIO %d HIGH (%s)\n", chain.pin, chain.name);
            digitalWrite(chain.pin, HIGH);
            delay(600);
            digitalWrite(chain.pin, LOW);
            delay(600);
        }
    }
    for (Chain &chain : chains)
    {
        chain.pixels.begin();
        chain.pixels.setBrightness(BRIGHTNESS);
        chain.pixels.clear();
        chain.pixels.show();
    }
}

namespace
{
    const char *pinName(int8_t pin)
    {
        return pin == ACTIVATE_PIN ? "Activate switch" : pin == FIRE_PIN ? "Fire button" : "unexpected pin";
    }

    void beginInputTest()
    {
        for (size_t i = 0; i < CANDIDATES; ++i)
        {
            pinMode(CANDIDATE_PINS[i], INPUT_PULLUP);
            delay(2);
            reported[i] = previous[i] = digitalRead(CANDIDATE_PINS[i]) == LOW;
        }
        Serial.printf("Input test: Activate GPIO %d, fire GPIO %d (closed = LOW). Watching", ACTIVATE_PIN, FIRE_PIN);
        for (size_t i = 0; i < CANDIDATES; ++i)
            Serial.printf(" %d", CANDIDATE_PINS[i]);
        Serial.println();
        for (size_t i = 0; i < CANDIDATES; ++i)
            if (reported[i])
                Serial.printf("  GPIO %d (%s) is CLOSED at start\n", CANDIDATE_PINS[i], pinName(CANDIDATE_PINS[i]));
    }

    bool closed(int8_t pin)
    {
        for (size_t i = 0; i < CANDIDATES; ++i)
            if (CANDIDATE_PINS[i] == pin)
                return reported[i];
        return false;
    }

    void updateInputTest()
    {
        // Report a change once it reads the same twice, 20 ms apart (debounce).
        for (size_t i = 0; i < CANDIDATES; ++i)
        {
            const bool now = digitalRead(CANDIDATE_PINS[i]) == LOW;
            if (now == previous[i] && now != reported[i])
            {
                reported[i] = now;
                Serial.printf("GPIO %d (%s): %s\n", CANDIDATE_PINS[i], pinName(CANDIDATE_PINS[i]),
                              now ? "CLOSED (on/pressed)" : "open (off/released)");
            }
            previous[i] = now;
        }

        Adafruit_NeoPixel &cyclotron = chains[0].pixels;
        Adafruit_NeoPixel &powerCell = chains[1].pixels;
        Adafruit_NeoPixel &wand = chains[2].pixels;
        cyclotron.fill(closed(ACTIVATE_PIN) ? cyclotron.Color(0, 255, 0) : 0);
        cyclotron.show();
        const bool fire = closed(FIRE_PIN);
        powerCell.fill(fire ? powerCell.Color(0, 0, 255) : 0);
        powerCell.show();
        wand.clear();
        for (uint16_t i = 7; i < 10; ++i)       // 3-pixel firing strip at the end of the chain
            wand.setPixelColor(i, fire ? wand.Color(255, 255, 255) : 0);
        wand.show();
        delay(20);
    }
}

void loop()
{
    if (INPUT_TEST)
    {
        static bool started = false;
        if (!started)
        {
            beginInputTest();
            started = true;
        }
        updateInputTest();
        return;
    }
    for (Chain &chain : chains)
        if (chain.enabled)
            test(chain);
}
