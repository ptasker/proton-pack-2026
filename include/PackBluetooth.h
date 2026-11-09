#pragma once

#include <Arduino.h>
#include <atomic>
#include <esp_a2dp_api.h>
#include <esp_avrc_api.h>
#include <esp_heap_caps.h>
#include <esp_bt.h>
#include <esp_bt_device.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include "Config.h"

// Bluetooth speaker mode: an A2DP sink (ESP-IDF Bluedroid API) whose PCM feeds
// the pack audio output. Wi-Fi and the SD mixer are off in this mode: pairing
// alone needs more RAM than is left beside the SD mixer. The phone's volume sets the level;
// pack master volume and mute still apply. Not a GPStar feature.
class PackBluetooth
{
public:
    // deviceName: the pack name (PackName.h), shared with the Wi-Fi network.
    bool begin(const String &deviceName)
    {
        instance = this;
        ring = xStreamBufferCreate(RING_BYTES, 1);
        if (ring == nullptr)
            return fail("ring allocation", ESP_ERR_NO_MEM);

        esp_bt_controller_config_t controller = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        controller.mode = ESP_BT_MODE_CLASSIC_BT;
        esp_err_t err = esp_bt_controller_init(&controller);
        if (err != ESP_OK)
            return fail("controller init", err);
        if ((err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK)
            return fail("controller enable", err);
        if ((err = esp_bluedroid_init()) != ESP_OK)
            return fail("Bluedroid init", err);
        if ((err = esp_bluedroid_enable()) != ESP_OK)
            return fail("Bluedroid enable", err);

        snprintf(name, sizeof(name), "%s", deviceName.c_str());
        esp_bt_dev_set_device_name(name);

        esp_bt_gap_register_callback(gapEvent);
        esp_bt_sp_param_t param = ESP_BT_SP_IOCAP_MODE;
        esp_bt_io_cap_t ioCap = ESP_BT_IO_CAP_NONE;     // "just works" pairing, no PIN
        esp_bt_gap_set_security_param(param, &ioCap, sizeof(ioCap));

        esp_bt_cod_t cod{};
        cod.major = ESP_BT_COD_MAJOR_DEV_AV;
        cod.minor = 0x05;                                 // loudspeaker
        cod.service = ESP_BT_COD_SRVC_RENDERING | ESP_BT_COD_SRVC_AUDIO;
        esp_bt_gap_set_cod(cod, ESP_BT_INIT_COD);

        // AVRCP controller + target: phones (iOS in particular) expect them beside
        // A2DP. No absolute-volume capability, so the phone scales the audio itself.
        esp_avrc_ct_register_callback(avrcControllerEvent);
        if ((err = esp_avrc_ct_init()) != ESP_OK)
            return fail("AVRCP controller init", err);
        esp_avrc_tg_register_callback(avrcTargetEvent);
        if ((err = esp_avrc_tg_init()) != ESP_OK)
            return fail("AVRCP target init", err);

        esp_a2d_register_callback(a2dpEvent);
        esp_a2d_sink_register_data_callback(audioData);
        if ((err = esp_a2d_sink_init()) != ESP_OK)
            return fail("A2DP sink init", err);
        setDiscoverable(true);
        running = true;
        Serial.printf("Bluetooth: speaker \"%s\" ready to pair (Wi-Fi off)\n", name);
        return true;
    }

    bool isRunning() const { return running; }

    // Audio task: adds Bluetooth audio to the mix. Small drift corrections (one
    // sample per block at most) keep the buffer centred, because the phone's
    // clock and the I2S clock differ slightly.
    void mix(int32_t *out, size_t frames, float gain)
    {
        if (!running || frames > MAX_FRAMES)
            return;
        const size_t buffered = xStreamBufferBytesAvailable(ring) / sizeof(int16_t);
        if (buffering)
        {
            if (buffered < START_FRAMES)
                return;
            buffering = false;
        }
        size_t want = frames;
        if (buffered > HIGH_FRAMES)
            want = frames + 1;      // drop one sample
        else if (buffered < LOW_FRAMES)
            want = frames - 1;      // repeat one sample
        const size_t got = xStreamBufferReceive(ring, scratch, want * sizeof(int16_t), 0) / sizeof(int16_t);
        if (got < want)
        {
            // Ran dry: play what arrived, then rebuffer.
            underruns.fetch_add(1);
            buffering = true;
            for (size_t i = 0; i < got && i < frames; ++i)
                out[i] += static_cast<int32_t>(scratch[i] * gain);
            return;
        }
        for (size_t i = 0; i < frames; ++i)
        {
            // Map output frames onto the `got` input samples (stretch or squeeze by one).
            out[i] += static_cast<int32_t>(scratch[(i * got) / frames] * gain);
        }
    }

    // Phone audio arrived in the last half second (used to duck pack effects).
    bool playing(uint32_t now) const
    {
        return running && now - lastAudioMs.load() < 500;
    }

    void update(uint32_t now)
    {
        // Low-heap warning (pairing allocates more; it failed below ~2 KB).
        const size_t lowest = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
        if (running && lowest < 16384 && now - lastHeapReport >= 10000)
        {
            lastHeapReport = now;
            Serial.printf("Bluetooth: LOW heap: free %u, lowest %u\n",
                          static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                          static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)));
        }
        const uint32_t count = underruns.load();
        if (count != reportedUnderruns && now - lastUnderrunReport >= 5000)
        {
            Serial.printf("Bluetooth: %lu audio buffer underruns\n", static_cast<unsigned long>(count));
            reportedUnderruns = count;
            lastUnderrunReport = now;
        }
    }

private:
    static constexpr size_t RING_BYTES = 16384;             // ~186 ms of mono 44.1 kHz
    static constexpr size_t START_FRAMES = 4096;            // start once ~93 ms is buffered
    static constexpr size_t HIGH_FRAMES = 5120;
    static constexpr size_t LOW_FRAMES = 3072;
    static constexpr size_t MAX_FRAMES = 256;
    static constexpr uint32_t OUTPUT_RATE = 44100;

    static PackBluetooth *instance;
    StreamBufferHandle_t ring = nullptr;
    bool running = false;
    bool buffering = true;                                   // audio task only
    int16_t scratch[MAX_FRAMES + 1] = {};                    // audio task only
    char name[25] = "ProtonPack";
    std::atomic<uint32_t> inputRate{44100};
    std::atomic<uint32_t> lastAudioMs{0};
    std::atomic<uint32_t> underruns{0};
    uint32_t reportedUnderruns = 0;
    uint32_t lastUnderrunReport = 0;
    uint32_t lastHeapReport = 0;
    // Bluetooth task only: 48 kHz to 44.1 kHz resampler state.
    float resamplePhase = 0.0f;
    int16_t previousSample = 0;
    int16_t converted[512] = {};

    bool fail(const char *step, esp_err_t err)
    {
        Serial.printf("Bluetooth: FAILED at %s (%s)\n", step, esp_err_to_name(err));
        return false;
    }

    static void setDiscoverable(bool on)
    {
        esp_bt_gap_set_scan_mode(on ? ESP_BT_CONNECTABLE : ESP_BT_NON_CONNECTABLE,
                                 on ? ESP_BT_GENERAL_DISCOVERABLE : ESP_BT_NON_DISCOVERABLE);
    }

    static void avrcControllerEvent(esp_avrc_ct_cb_event_t, esp_avrc_ct_cb_param_t *) {}
    static void avrcTargetEvent(esp_avrc_tg_cb_event_t, esp_avrc_tg_cb_param_t *) {}

    static void gapEvent(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
    {
        if (event == ESP_BT_GAP_CFM_REQ_EVT)
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        else if (event == ESP_BT_GAP_AUTH_CMPL_EVT)
            Serial.printf("Bluetooth: pairing with %s %s\n",
                          reinterpret_cast<const char *>(param->auth_cmpl.device_name),
                          param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS ? "succeeded" : "failed");
    }

    static void a2dpEvent(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
    {
        if (event == ESP_A2D_CONNECTION_STATE_EVT)
        {
            const esp_a2d_connection_state_t state = param->conn_stat.state;
            if (state == ESP_A2D_CONNECTION_STATE_CONNECTED)
            {
                Serial.println("Bluetooth: phone connected");
                setDiscoverable(false);   // one phone at a time
            }
            else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
            {
                Serial.println("Bluetooth: phone disconnected; ready to pair");
                setDiscoverable(true);
            }
        }
        else if (event == ESP_A2D_AUDIO_STATE_EVT)
            Serial.printf("Bluetooth: audio %s\n",
                          param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED ? "started" : "paused");
        else if (event == ESP_A2D_AUDIO_CFG_EVT && param->audio_cfg.mcc.type == ESP_A2D_MCT_SBC)
        {
            const uint8_t octet = param->audio_cfg.mcc.cie.sbc[0];
            const uint32_t rate = (octet & 0x40) ? 32000 : (octet & 0x20) ? 44100 : (octet & 0x10) ? 48000 : 16000;
            instance->inputRate.store(rate);
            instance->resamplePhase = 0.0f;
            Serial.printf("Bluetooth: SBC stream at %lu Hz%s\n", static_cast<unsigned long>(rate),
                          rate == OUTPUT_RATE ? "" : " (resampled to 44.1 kHz)");
        }
    }

    // Bluetooth task: interleaved 16-bit stereo PCM from the SBC decoder.
    static void audioData(const uint8_t *data, uint32_t length)
    {
        PackBluetooth &self = *instance;
        self.lastAudioMs.store(millis());
        const int16_t *pcm = reinterpret_cast<const int16_t *>(data);
        const size_t frames = length / 4;
        const uint32_t rate = self.inputRate.load();
        size_t done = 0;
        while (done < frames)
        {
            size_t count = 0;
            if (rate == OUTPUT_RATE)
            {
                for (; done < frames && count < sizeof(self.converted) / sizeof(int16_t); ++done)
                    self.converted[count++] =
                        static_cast<int16_t>((static_cast<int32_t>(pcm[done * 2]) + pcm[done * 2 + 1]) >> 1);
            }
            else
            {
                // Linear interpolation between consecutive input samples.
                const float step = static_cast<float>(rate) / OUTPUT_RATE;
                for (; done < frames && count < sizeof(self.converted) / sizeof(int16_t) - 2; ++done)
                {
                    const int16_t sample =
                        static_cast<int16_t>((static_cast<int32_t>(pcm[done * 2]) + pcm[done * 2 + 1]) >> 1);
                    while (self.resamplePhase < 1.0f)
                    {
                        self.converted[count++] = static_cast<int16_t>(
                            self.previousSample + (sample - self.previousSample) * self.resamplePhase);
                        self.resamplePhase += step;
                    }
                    self.resamplePhase -= 1.0f;
                    self.previousSample = sample;
                }
            }
            // Drop audio rather than block the Bluetooth stack if the ring is full.
            xStreamBufferSend(self.ring, self.converted, count * sizeof(int16_t), 0);
        }
    }
};

PackBluetooth *PackBluetooth::instance = nullptr;
