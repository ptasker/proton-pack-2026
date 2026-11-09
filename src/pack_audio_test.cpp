#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <driver/i2s.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <math.h>
#include "Config.h"

static_assert(config::AUDIO_BRINGUP_ENABLED, "The audio-only target requires AUDIO_BRINGUP_ENABLED");

constexpr i2s_port_t AUDIO_PORT = I2S_NUM_0;
constexpr uint32_t SAMPLE_RATE = 44100;
constexpr uint32_t BLOCK_FRAMES = 256;
constexpr uint32_t TONE_HZ = 1000;
constexpr int16_t TONE_PEAK = 8000;

bool driverInstalled = false;
bool audioReady = false;
bool toneActive = false;
Adafruit_NeoPixel ring{config::RING_PIXELS, config::RING_PIN, NEO_GRB + NEO_KHZ800};
Adafruit_NeoPixel powerBar{
    config::POWER_BAR_PIN >= 0 ? config::POWER_BAR_PIXELS : 0,
    config::POWER_BAR_PIN, NEO_GRB + NEO_KHZ800};

void holdAudioPinsLow()
{
    const int8_t pins[] = {config::AUDIO_BCLK_PIN, config::AUDIO_LRCLK_PIN, config::AUDIO_DATA_PIN};
    for (int8_t pin : pins)
    {
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
    }
}

void failAudio(esp_err_t error, const char *operation)
{
    audioReady = false;
    toneActive = false;
    if (driverInstalled)
    {
        const esp_err_t stopped = i2s_stop(AUDIO_PORT);
        const esp_err_t removed = i2s_driver_uninstall(AUDIO_PORT);
        driverInstalled = false;
        if (stopped != ESP_OK || removed != ESP_OK)
            Serial.printf("Audio cleanup: stop=%s uninstall=%s\n",
                          esp_err_to_name(stopped), esp_err_to_name(removed));
    }
    holdAudioPinsLow();
    Serial.printf("Audio FAILED during %s: %s; disconnect amplifier supply and reset to retry.\n",
                  operation, esp_err_to_name(error));
}

bool checkAudio(esp_err_t error, const char *operation)
{
    if (error == ESP_OK)
        return true;
    failAudio(error, operation);
    return false;
}

void printAudioTestHelp()
{
    Serial.println("Pack BASIC I2S diagnostic: direct loop; steady blue power cell; no animations or wand UART.");
    Serial.println("Serial: t=continuous tone, m=digital silence, l=resend static lights, ?=help/status.");
}

void printAudioStatus()
{
    if (!audioReady)
        Serial.println("Audio: FAILED; disconnect amplifier supply and reset to retry.");
    else
        Serial.println(toneActive ? "Audio: playing continuous tone; m=silence" :
                                   "Audio: streaming digital silence; t=start tone");
    Serial.printf("I2S: BCLK=%d LRCLK=%d DIN=%d, %u Hz, 16-bit stereo, %u-frame blocks\n",
                  static_cast<int>(config::AUDIO_BCLK_PIN),
                  static_cast<int>(config::AUDIO_LRCLK_PIN),
                  static_cast<int>(config::AUDIO_DATA_PIN),
                  static_cast<unsigned>(SAMPLE_RATE), static_cast<unsigned>(BLOCK_FRAMES));
    Serial.printf("Tone: %u Hz, peak %d/32767 (about -12 dBFS); runs until m\n",
                  static_cast<unsigned>(TONE_HZ), static_cast<int>(TONE_PEAK));
}

void setup()
{
    Serial.begin(115200);
    ring.begin();
    ring.clear();
    ring.show();
    if (config::POWER_BAR_PIN >= 0)
    {
        powerBar.begin();
        powerBar.setBrightness(config::POWER_BAR_BRIGHTNESS);
        powerBar.fill(powerBar.Color(0, 0, 255));
        powerBar.show();
    }
    else
        Serial.println("Power cell disabled; no battery-load lighting available.");
    printAudioTestHelp();

    holdAudioPinsLow();
    i2s_config_t settings{};
    settings.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    settings.sample_rate = SAMPLE_RATE;
    settings.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    settings.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    settings.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    settings.intr_alloc_flags = 0;
    settings.dma_buf_count = 8;
    settings.dma_buf_len = BLOCK_FRAMES;
    settings.use_apll = false;
    settings.tx_desc_auto_clear = true;

    if (!checkAudio(i2s_driver_install(AUDIO_PORT, &settings, 0, nullptr), "driver install"))
        return;
    driverInstalled = true;
    if (!checkAudio(i2s_zero_dma_buffer(AUDIO_PORT), "clear DMA") ||
        !checkAudio(i2s_stop(AUDIO_PORT), "stop before pin routing"))
        return;

    i2s_pin_config_t pins{};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = config::AUDIO_BCLK_PIN;
    pins.ws_io_num = config::AUDIO_LRCLK_PIN;
    pins.data_out_num = config::AUDIO_DATA_PIN;
    pins.data_in_num = I2S_PIN_NO_CHANGE;
    if (!checkAudio(i2s_set_pin(AUDIO_PORT, &pins), "pin routing") ||
        !checkAudio(i2s_start(AUDIO_PORT), "start silent stream"))
        return;

    audioReady = true;
    printAudioStatus();
}

void loop()
{
    static int16_t buffer[BLOCK_FRAMES * 2];
    static float phase = 0.0f;
    constexpr float phaseLimit = 2.0f * PI;
    constexpr float phaseStep = phaseLimit * TONE_HZ / SAMPLE_RATE;

    for (uint8_t handled = 0; handled < 16 && Serial.available() > 0; ++handled)
    {
        switch (Serial.read())
        {
        case 't':
            if (!audioReady)
                printAudioStatus();
            else if (toneActive)
                Serial.println("Audio: tone already running; m=silence.");
            else
            {
                phase = 0.0f;
                toneActive = true;
                Serial.println("Audio: continuous tone started; m=silence.");
            }
            break;
        case 'm':
            toneActive = false;
            if (audioReady)
                Serial.println("Audio: silence requested; allow about 46 ms for queued audio.");
            else
                printAudioStatus();
            break;
        case 'l':
            if (toneActive)
            {
                Serial.println("Stop the tone with m before resending lights.");
                break;
            }
            ring.show();
            if (config::POWER_BAR_PIN >= 0)
            {
                powerBar.show();
                Serial.println("Static lights resent: cyclotron off, power cell steady blue.");
            }
            else
                Serial.println("Power cell disabled; no battery-load lighting available.");
            break;
        case '?':
            printAudioTestHelp();
            printAudioStatus();
            break;
        case '\r':
        case '\n':
        case ' ':
            break;
        default:
            Serial.println("Basic I2S diagnostic: unsupported command; use t, m, l, or ?.");
            break;
        }
    }

    if (!audioReady)
    {
        delay(1);
        return;
    }

    for (uint32_t frame = 0; frame < BLOCK_FRAMES; ++frame)
    {
        int16_t sample = 0;
        if (toneActive)
        {
            sample = static_cast<int16_t>(TONE_PEAK * sinf(phase));
            phase += phaseStep;
            if (phase >= phaseLimit)
                phase -= phaseLimit;
        }
        buffer[frame * 2] = sample;
        buffer[frame * 2 + 1] = sample;
    }

    size_t written = 0;
    const esp_err_t result = i2s_write(AUDIO_PORT, buffer, sizeof(buffer), &written,
                                     pdMS_TO_TICKS(100));
    if (result != ESP_OK || written != sizeof(buffer))
        failAudio(result != ESP_OK ? result : ESP_ERR_TIMEOUT, "DMA write/short write");
}
