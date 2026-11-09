#pragma once

#include <Arduino.h>
#include <atomic>
#include <math.h>
#include <driver/i2s.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Config.h"
#include "PackBluetooth.h"
#include "PackSdAudio.h"

class PackAudio
{
public:
    // Must be called before begin(); the I2S task mixes this source with tones.
    // Bluetooth speaker mode: phone audio joins the mix at the music level.
    void attachBluetoothSource(PackBluetooth *source) { btSource = source; }

    void attachSdSource(PackSdAudio *source)
    {
        sdSource = source;
    }

    void begin()
    {
        if (!config::AUDIO_BRINGUP_ENABLED)
            return;

        holdPinsLow();
        i2s_config_t settings{};
        settings.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
        settings.sample_rate = SAMPLE_RATE;
        settings.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
        settings.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
        settings.communication_format = I2S_COMM_FORMAT_STAND_I2S;
        settings.dma_buf_count = 8;
        settings.dma_buf_len = BLOCK_FRAMES;
        settings.tx_desc_auto_clear = true;
        settings.use_apll = false;

        if (!check(i2s_driver_install(PORT, &settings, 0, nullptr), "driver install"))
            return;
        installed = true;
        if (!check(i2s_zero_dma_buffer(PORT), "clear DMA") ||
            !check(i2s_stop(PORT), "stop before pin routing"))
            return;

        i2s_pin_config_t pins{};
        pins.mck_io_num = I2S_PIN_NO_CHANGE;
        pins.bck_io_num = config::AUDIO_BCLK_PIN;
        pins.ws_io_num = config::AUDIO_LRCLK_PIN;
        pins.data_out_num = config::AUDIO_DATA_PIN;
        pins.data_in_num = I2S_PIN_NO_CHANGE;
        if (!check(i2s_set_pin(PORT, &pins), "pin routing") ||
            !check(i2s_start(PORT), "start silent stream"))
            return;

        state.store(State::Silence);
        // Above the SD reader (priority 2, also core 0) so output is never starved.
        if (xTaskCreatePinnedToCore(taskEntry, "pack-audio", 4096, this, 3, nullptr, 0) != pdPASS)
        {
            fail(ESP_ERR_NO_MEM, "audio task creation");
            return;
        }
        if (config::AUDIO_BOOT_BEEP)
        {
            request.store(Request::Beep);
            Serial.println("Audio: boot beep (1 kHz, 200 ms)");
        }
    }

    void update(uint32_t now)
    {
        const State current = state.load();
        if (current != lastReported)
        {
            lastReported = current;
            printStatus();
        }

        // Clipping means the mixed tracks exceed full scale; lower the volume.
        const uint32_t count = clippedSamples.load();
        if (count != reportedClips && now - lastClipReport >= 1000)
        {
            Serial.printf("Audio: %lu clipped samples despite the limiter (try - for lower volume)\n",
                          static_cast<unsigned long>(count));
            reportedClips = count;
            lastClipReport = now;
        }

        // Diagnostic: how often the limiter cuts in (each attack is a gain step).
        const uint32_t limited = limitedBlocks.load();
        if (limited != reportedLimited && now - lastLimitReport >= 1000)
        {
            Serial.printf("Audio: limiter active in %lu blocks (deepest -%.1f dB)\n",
                          static_cast<unsigned long>(limited),
                          deepestLimitCentiDb.load() / 100.0f);
            reportedLimited = limited;
            lastLimitReport = now;
        }
    }

    uint32_t clipCount() const { return clippedSamples.load(); }
    uint32_t limitedCount() const { return limitedBlocks.load(); }

    // Fades all SD sounds out (or back in). Tones and confirmation beeps still play.
    void setSdMuted(bool muted)
    {
        sdMuted.store(muted);
    }

    // Short confirmation beep: high when unmuting, low when muting.
    void requestFeedbackBeep(bool high)
    {
        Request expected = Request::None;
        request.compare_exchange_strong(expected, high ? Request::Beep : Request::LowBeep);
    }

    void requestTone()
    {
        Request expected = Request::None;
        if (state.load() != State::Silence ||
            !request.compare_exchange_strong(expected, Request::Tone))
        {
            Serial.println("Audio: tone ignored; not ready, already playing, or command pending");
            return;
        }
        Serial.println("Audio: requested 1-second 440 Hz tone at low digital level");
    }

    void silence()
    {
        const State current = state.load();
        if (current != State::Silence && current != State::Tone)
        {
            Serial.println("Audio: stream unavailable; keep amplifier SD grounded");
            return;
        }
        request.store(Request::Silence);
        Serial.println("Audio: silence requested (short fade plus queued DMA audio)");
    }

    void printStatus() const
    {
        const char *text = "disabled; keep amplifier SD grounded";
        switch (state.load())
        {
        case State::Silence:
            text = "streaming digital silence; t=test tone, m=silence";
            break;
        case State::Tone:
            text = "playing tone/beep; returns to silence automatically";
            break;
        case State::Failed:
            text = "FAILED; keep amplifier SD grounded and reset to retry";
            break;
        case State::Disabled:
            break;
        }
        Serial.printf("Audio: %s\n", text);
        Serial.printf("I2S: BCLK=%d LRCLK=%d DIN=%d, 44100 Hz, 16-bit stereo\n",
                      static_cast<int>(config::AUDIO_BCLK_PIN),
                      static_cast<int>(config::AUDIO_LRCLK_PIN),
                      static_cast<int>(config::AUDIO_DATA_PIN));
    }

private:
    enum class State : uint8_t { Disabled, Silence, Tone, Failed };
    enum class Request : uint8_t { None, Tone, Silence, Beep, LowBeep };
    static constexpr i2s_port_t PORT = I2S_NUM_0;
    static constexpr uint32_t SAMPLE_RATE = 44100;
    static constexpr uint32_t BLOCK_FRAMES = 128;
    const float BT_GAIN = powf(10.0f, config::MUSIC_LEVEL_DB / 20.0f);
    static constexpr uint32_t TONE_FRAMES = SAMPLE_RATE;
    static constexpr uint32_t FADE_FRAMES = SAMPLE_RATE / 100;
    static constexpr float TONE_HZ = 440.0f;
    static constexpr int16_t TONE_PEAK = 1024;
    // Boot beep matches the 1 kHz / 8000-peak tone confirmed clean on USB 5V.
    static constexpr uint32_t BEEP_FRAMES = SAMPLE_RATE / 5;
    static constexpr float BEEP_HZ = 1000.0f;
    static constexpr int16_t BEEP_PEAK = 8000;
    static constexpr float LOW_BEEP_HZ = 500.0f;
    static constexpr float MUTE_STEP = 1.0f / 20;   // per 2.9 ms block: ~60 ms mute fade
    std::atomic<bool> sdMuted{false};
    float sdGain = 1.0f;                            // audio task only
    static_assert(BLOCK_FRAMES <= PackSdAudio::MAX_MIX_FRAMES, "SD mixer block too small");
    std::atomic<State> state{State::Disabled};
    std::atomic<Request> request{Request::None};
    static constexpr int32_t LIMIT_PEAK = 32000;      // limiter ceiling, just under full scale
    static constexpr float LIMIT_RELEASE = 0.01f;    // per 2.9 ms block: ~0.3 s recovery
    float limiterGain = 1.0f;                        // audio task only
    std::atomic<uint32_t> clippedSamples{0};
    std::atomic<uint32_t> limitedBlocks{0};
    std::atomic<uint32_t> deepestLimitCentiDb{0};
    uint32_t reportedLimited = 0;
    uint32_t lastLimitReport = 0;
    uint32_t reportedClips = 0;
    uint32_t lastClipReport = 0;
    State lastReported = State::Disabled;
    bool installed = false;
    PackSdAudio *sdSource = nullptr;
    PackBluetooth *btSource = nullptr;

    static void taskEntry(void *context)
    {
        static_cast<PackAudio *>(context)->stream();
        vTaskDelete(nullptr);
    }

    void stream()
    {
        int16_t samples[BLOCK_FRAMES * 2] = {};
        uint32_t position = 0;
        uint32_t toneEnd = 0;
        float toneHz = TONE_HZ;
        float tonePeak = TONE_PEAK;
        for (;;)
        {
            const Request command = request.exchange(Request::None);
            if ((command == Request::Tone || command == Request::Beep || command == Request::LowBeep) &&
                position >= toneEnd)
            {
                const bool beep = command != Request::Tone;
                position = 0;
                toneEnd = beep ? BEEP_FRAMES : TONE_FRAMES;
                toneHz = command == Request::LowBeep ? LOW_BEEP_HZ : beep ? BEEP_HZ : TONE_HZ;
                tonePeak = beep ? BEEP_PEAK : TONE_PEAK;
                state.store(State::Tone);
            }
            else if (command == Request::Silence && toneEnd > position + FADE_FRAMES)
            {
                toneEnd = position + FADE_FRAMES;
            }

            int32_t mixed[BLOCK_FRAMES] = {};
            if (sdSource != nullptr)
                sdSource->mix(mixed, BLOCK_FRAMES);
            if (btSource != nullptr)
                btSource->mix(mixed, BLOCK_FRAMES, BT_GAIN);

            // Mute ramps the SD mix only; the tone below stays audible for feedback.
            const float sdTarget = sdMuted.load() ? 0.0f : 1.0f;
            const float sdStart = sdGain;
            if (sdGain < sdTarget)
                sdGain = sdGain + MUTE_STEP < sdTarget ? sdGain + MUTE_STEP : sdTarget;
            else if (sdGain > sdTarget)
                sdGain = sdGain - MUTE_STEP > sdTarget ? sdGain - MUTE_STEP : sdTarget;
            if (sdStart != 1.0f || sdGain != 1.0f)
            {
                const float sdStep = (sdGain - sdStart) / BLOCK_FRAMES;
                float gain = sdStart;
                for (uint32_t frame = 0; frame < BLOCK_FRAMES; ++frame, gain += sdStep)
                    mixed[frame] = static_cast<int32_t>(mixed[frame] * gain);
            }

            int32_t peak = 0;
            for (uint32_t frame = 0; frame < BLOCK_FRAMES; ++frame)
            {
                if (position < toneEnd)
                {
                    uint32_t envelope = position < FADE_FRAMES ? position : FADE_FRAMES;
                    const uint32_t remaining = toneEnd - position;
                    if (remaining < envelope)
                        envelope = remaining;
                    const float phase = 6.28318530718f * toneHz * position / SAMPLE_RATE;
                    mixed[frame] += static_cast<int32_t>(sinf(phase) * tonePeak * envelope / FADE_FRAMES);
                    ++position;
                }
                const int32_t magnitude = mixed[frame] < 0 ? -mixed[frame] : mixed[frame];
                if (magnitude > peak)
                    peak = magnitude;
            }

            // Peak limiter: when several loud tracks sum past full scale, drop the
            // gain for the whole block (the block is known before it is played),
            // then recover smoothly over a few hundred milliseconds.
            const float target = peak > LIMIT_PEAK ? static_cast<float>(LIMIT_PEAK) / peak : 1.0f;
            if (peak > LIMIT_PEAK)
            {
                limitedBlocks.fetch_add(1);
                const uint32_t centiDb = static_cast<uint32_t>(-2000.0f * log10f(target));
                if (centiDb > deepestLimitCentiDb.load())
                    deepestLimitCentiDb.store(centiDb);
            }
            const float startGain = limiterGain;
            float endGain = startGain + (1.0f - startGain) * LIMIT_RELEASE;
            if (endGain > target)
                endGain = target;
            const bool attacking = endGain < startGain;
            const float step = attacking ? 0.0f : (endGain - startGain) / BLOCK_FRAMES;
            float gain = attacking ? endGain : startGain;
            limiterGain = endGain;
            for (uint32_t frame = 0; frame < BLOCK_FRAMES; ++frame, gain += step)
            {
                const int32_t sample = static_cast<int32_t>(mixed[frame] * gain);
                if (sample > 32767 || sample < -32768)
                    clippedSamples.fetch_add(1);
                const int16_t clipped = static_cast<int16_t>(constrain(sample, -32768, 32767));
                samples[frame * 2] = clipped;
                samples[frame * 2 + 1] = clipped;
            }

            size_t written = 0;
            const esp_err_t result = i2s_write(PORT, samples, sizeof(samples), &written,
                                               pdMS_TO_TICKS(100));
            if (result != ESP_OK || written != sizeof(samples))
            {
                fail(result != ESP_OK ? result : ESP_ERR_TIMEOUT, "DMA write/short write");
                return;
            }
            if (position >= toneEnd)
                state.store(State::Silence);
        }
    }

    static void holdPinsLow()
    {
        const int8_t pins[] = {config::AUDIO_BCLK_PIN, config::AUDIO_LRCLK_PIN, config::AUDIO_DATA_PIN};
        for (int8_t pin : pins)
        {
            digitalWrite(pin, LOW);
            pinMode(pin, OUTPUT);
        }
    }

    bool check(esp_err_t error, const char *operation)
    {
        if (error == ESP_OK)
            return true;
        fail(error, operation);
        return false;
    }

    void fail(esp_err_t error, const char *operation)
    {
        state.store(State::Failed);
        if (installed)
        {
            const esp_err_t stopped = i2s_stop(PORT);
            const esp_err_t removed = i2s_driver_uninstall(PORT);
            installed = false;
            if (stopped != ESP_OK || removed != ESP_OK)
                Serial.printf("Audio cleanup: stop=%s uninstall=%s\n",
                              esp_err_to_name(stopped), esp_err_to_name(removed));
        }
        holdPinsLow();
        Serial.printf("Audio FAILED during %s: %s; keep amp SD grounded. Wand/pack lights remain active.\n",
                      operation, esp_err_to_name(error));
    }
};
