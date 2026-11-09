#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <algorithm>
#include <atomic>
#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/stream_buffer.h>
#include <freertos/task.h>
#include "Config.h"

// GPStar Audio-style SD player. Indexes GPStar-numbered WAVs (NNN_name.wav in
// the card root) and plays up to VOICES (8) of them at once, each with its own loop
// flag, gain in dB and linear-dB fades (play/stop/fade like GPStar's
// trackPlayPoly/trackStop/trackFade). A reader task streams each file into its
// own buffer; PackAudio's I2S task calls mix().
class PackSdAudio
{
public:
    static constexpr int8_t MIN_DB = -70;          // GPStar i_volume_abs_min; treated as silent
    static constexpr size_t MAX_MIX_FRAMES = 128;  // largest block mix() accepts

    void begin()
    {
        if (!config::SD_AUDIO_ENABLED)
        {
            Serial.println("SD audio: disabled in Config.h");
            return;
        }
        // Mount first: FATFS needs one large contiguous block (about 4.6 KB per
        // allowed open file with this core's 4 KB sectors) before the voice buffers
        // and Wi-Fi fragment the heap.
        if (mount())
            buildIndex(false);
        for (Voice &voice : voices)
        {
            voice.ring = xStreamBufferCreate(RING_BYTES, 1);
            if (voice.ring == nullptr)
            {
                Serial.println("SD audio: FAILED to allocate voice buffers");
                return;
            }
        }
        // Room for a burst of events in one loop pass (e.g. wand off while firing).
        commands = xQueueCreate(32, sizeof(Command));
        if (commands == nullptr)
        {
            Serial.println("SD audio: FAILED to allocate command queue");
            return;
        }
        setVolumeDb(config::SD_AUDIO_DEFAULT_DB);
        // Core 0 is otherwise only the I2S task (priority 3); the Arduino loop on core 1
        // never yields, so sharing its core and priority starved SD reads.
        if (xTaskCreatePinnedToCore(readerEntry, "pack-sd", 4096, this, 2, nullptr, 0) != pdPASS)
        {
            Serial.println("SD audio: FAILED to start reader task");
            return;
        }
        ready = true;
    }

    // Loop-task status reporting.
    void update(uint32_t now)
    {
        const uint32_t count = underruns.load();
        if (count != reportedUnderruns && now - lastUnderrunReport >= 1000)
        {
            Serial.printf("SD audio: %lu buffer underruns (SD read too slow)\n",
                          static_cast<unsigned long>(count));
            reportedUnderruns = count;
            lastUnderrunReport = now;
        }
    }

    // GPStar playEffect(): optional loop, gain in dB, optional fade-in from MIN_DB.
    void play(uint16_t track, bool loop = false, int gainDb = 0, uint16_t fadeInMs = 0)
    {
        if (!ready || !mounted)
        {
            Serial.printf("SD audio: cannot play %03u: %s\n", static_cast<unsigned>(track),
                          ready ? "no card mounted (d=retry)" : "unavailable");
            return;
        }
        const auto entry = std::find_if(tracks.begin(), tracks.end(),
                                        [track](const Track &t) { return t.number == track; });
        if (entry == tracks.end())
        {
            Serial.printf("SD audio: track %03u not found on card\n", static_cast<unsigned>(track));
            return;
        }

        Command command{};
        command.type = CommandType::Play;
        command.track = track;
        command.loop = loop;
        command.gainDb = clampDb(gainDb);
        command.ms = fadeInMs;
        const int length = snprintf(command.path, sizeof(command.path), "/sd/%s", nameOf(*entry));
        if (length < 0 || static_cast<size_t>(length) >= sizeof(command.path))
        {
            Serial.printf("SD audio: file name too long for track %03u\n", static_cast<unsigned>(track));
            return;
        }
        send(command);
    }

    // GPStar stopEffect(). A 5 ms ramp avoids a click.
    void stop(uint16_t track)
    {
        fade(track, MIN_DB, STOP_MS, true);
    }

    // GPStar fadeoutEffect(): fade to silence, then stop.
    void fadeout(uint16_t track, uint16_t ms = 50)
    {
        fade(track, MIN_DB, ms, true);
    }

    // GPStar trackFade(): every voice playing `track` (0 = all voices).
    void fade(uint16_t track, int targetDb, uint16_t ms, bool stopAtEnd)
    {
        if (!ready || !mounted)
            return;
        Command command{};
        command.type = CommandType::Fade;
        command.track = track;
        command.gainDb = clampDb(targetDb);
        command.ms = ms;
        command.stopAtEnd = stopAtEnd;
        send(command);
    }

    void stopAll()
    {
        stop(0);
    }

    // Extra per-track volume trim on top of play/fade gains (not a GPStar feature):
    // every voice of `track`, including ones started later, slides to `db` over
    // about TRIM_RAMP_MS without disturbing fades in progress. 0 dB clears it.
    void setTrim(uint16_t track, int db)
    {
        if (track == 0)
            return;
        const int32_t gain = static_cast<int32_t>(dbToGain(clampDb(db)) * 32767.0f);
        size_t slot = TRIMS;
        for (size_t i = 0; i < TRIMS; ++i)
        {
            if (trimTrack[i].load() == track)
            {
                slot = i;
                break;
            }
            if (slot == TRIMS && trimTrack[i].load() == 0)
                slot = i;
        }
        if (db >= 0)
        {
            // 0 dB frees the slot; voices slide back to full volume on their own.
            if (slot != TRIMS && trimTrack[slot].load() == track)
                trimTrack[slot].store(0);
            return;
        }
        if (slot == TRIMS)
        {
            Serial.println("SD audio: no free trim slot");
            return;
        }
        trimQ15[slot].store(gain);
        trimTrack[slot].store(track);
    }

    // Trim for every effect track (below 500), e.g. ducking effects under music.
    void setEffectsTrim(int db)
    {
        effectsTrimQ15.store(static_cast<int32_t>(dbToGain(clampDb(db)) * 32767.0f));
    }

    // GPStar stopEffectLoop(): a looping track finishes its current pass, then ends.
    void endLoop(uint16_t track)
    {
        if (!ready || !mounted)
            return;
        Command command{};
        command.type = CommandType::EndLoop;
        command.track = track;
        send(command);
    }

    // Mount if needed, rescan, and print every indexed track.
    void listTracks()
    {
        if (!ready)
        {
            Serial.println("SD audio: unavailable");
            return;
        }
        if (mount())
            buildIndex(true);
    }

    void setVolume(int db)
    {
        setVolumeDb(db);
        Serial.printf("SD audio: master volume %d dB\n", static_cast<int>(volumeDb));
    }

    // Read-only status for the loop task (web page).
    bool cardMounted() const { return mounted; }
    size_t trackCount() const { return tracks.size(); }
    int8_t volume() const { return volumeDb; }
    uint32_t indexVersion() const { return indexGeneration; }
    uint32_t underrunCount() const { return underruns.load(); }

    template <typename F>
    void forEachTrack(F visit) const
    {
        for (const Track &t : tracks)
            visit(t.number, nameOf(t));
    }

    // A music track (500+) currently playing, or 0. Approximate snapshot.
    uint16_t playingMusicTrack() const
    {
        for (const Voice &voice : voices)
            if (voice.state.load() != VOICE_FREE && voice.track >= 500)
                return voice.track;
        return 0;
    }

    void changeVolume(int8_t deltaDb)
    {
        setVolumeDb(volumeDb + deltaDb);
        Serial.printf("SD audio: master volume %d dB\n", static_cast<int>(volumeDb));
    }

    void printStatus() const
    {
        if (!config::SD_AUDIO_ENABLED)
            return;
        Serial.printf("SD audio: %s, %u tracks, master volume %d dB\n",
                      mounted ? "card mounted" : "NO CARD", static_cast<unsigned>(tracks.size()),
                      static_cast<int>(volumeDb));
        for (size_t i = 0; i < VOICES; ++i)
        {
            const uint8_t state = voices[i].state.load();
            if (state != VOICE_FREE)
                Serial.printf("  voice %u: track %03u%s, %s\n", static_cast<unsigned>(i),
                              static_cast<unsigned>(voices[i].track), voices[i].loop ? " (loop)" : "",
                              state == VOICE_LOADING ? "loading" : state == VOICE_PLAYING ? "playing" : "ending");
        }
        Serial.printf("SD pins: SCK=%d MISO=%d MOSI=%d CS=%d, %lu Hz\n",
                      static_cast<int>(config::SD_SCK_PIN), static_cast<int>(config::SD_MISO_PIN),
                      static_cast<int>(config::SD_MOSI_PIN), static_cast<int>(config::SD_CS_PIN),
                      static_cast<unsigned long>(config::SD_SPI_HZ));
    }

    // Called from the I2S task: adds every playing voice, with fades and master
    // volume applied, into `out` (mono, 32-bit accumulator).
    void mix(int32_t *out, size_t frames)
    {
        if (frames > MAX_MIX_FRAMES)
            frames = MAX_MIX_FRAMES;
        const float master = masterGain.load();
        for (Voice &voice : voices)
        {
            if (voice.state.load() != VOICE_PLAYING)
                continue;
            applyPendingControl(voice);

            const size_t received = xStreamBufferReceive(voice.ring, scratch, frames * sizeof(int16_t), 0) /
                                    sizeof(int16_t);
            const float startDb = voice.currentDb;
            if (voice.fadeFrames != 0)
            {
                voice.fadePos += frames;
                if (voice.fadePos >= voice.fadeFrames)
                {
                    voice.fadeFrames = 0;
                    voice.currentDb = voice.targetDb;
                }
                else
                    voice.currentDb = voice.startDb +
                                      (voice.targetDb - voice.startDb) * voice.fadePos / voice.fadeFrames;
            }

            const float trimStart = voice.trim;
            const float trimTarget = trimFor(voice.track);
            const float trimLimit = TRIM_SLEW_PER_FRAME * frames;
            if (trimTarget > voice.trim)
                voice.trim = trimTarget - voice.trim > trimLimit ? voice.trim + trimLimit : trimTarget;
            else
                voice.trim = voice.trim - trimTarget > trimLimit ? voice.trim - trimLimit : trimTarget;

            const float startGain = dbToGain(startDb) * master * trimStart;
            const float step = (dbToGain(voice.currentDb) * master * voice.trim - startGain) / frames;
            float gain = startGain;
            for (size_t i = 0; i < received; ++i, gain += step)
                out[i] += static_cast<int32_t>(scratch[i] * gain);

            if (voice.stopAtEnd && voice.fadeFrames == 0)
                release(voice, false);
            else if (received < frames)
            {
                if (voice.fileDone.load() && xStreamBufferIsEmpty(voice.ring) == pdTRUE)
                    release(voice, true);
                else
                    underruns.fetch_add(1);
            }
        }
    }

private:
    static constexpr size_t VOICES = 8;
    // Sized to leave ~40 KB free with Wi-Fi on (16 KB rings starved the heap).
    static constexpr size_t RING_BYTES = 8192;       // ~93 ms of mono 44.1 kHz audio per voice
    static constexpr size_t PREFILL_BYTES = 4096;    // start a voice once ~46 ms is buffered
    static constexpr size_t CHUNK_BYTES = 4096;      // max bytes read from a file per pass
    static constexpr size_t MIN_FILL_FRAMES = 1024;  // read only when this much buffer is free
    static constexpr uint32_t SAMPLE_RATE = 44100;
    static constexpr uint16_t STOP_MS = 5;
    static constexpr uint8_t VOICE_FREE = 0;
    static constexpr uint8_t VOICE_LOADING = 1;      // reader prefilling; mixer ignores
    static constexpr uint8_t VOICE_PLAYING = 2;      // mixer consuming
    static constexpr uint8_t VOICE_RELEASING = 3;    // mixer finished; reader reclaims

    enum class CommandType : uint8_t { Play, Fade, EndLoop };
    struct Command
    {
        CommandType type;
        uint16_t track;
        bool loop;
        bool stopAtEnd;
        int8_t gainDb;
        uint16_t ms;
        char path[80];
    };
    // Track names live in one packed buffer (446 separate Strings cost ~10 KB more).
    struct Track
    {
        uint16_t number;
        uint16_t nameOffset;
    };
    struct Voice
    {
        std::atomic<uint8_t> state{VOICE_FREE};
        std::atomic<bool> fileDone{true};
        StreamBufferHandle_t ring = nullptr;
        // Reader-owned.
        uint16_t track = 0;
        bool loop = false;
        bool naturalEnd = false;
        int file = -1;                                      // POSIX fd: avoids stdio's tiny buffer
        uint32_t dataStart = 0;
        uint32_t dataBytes = 0;
        uint32_t dataRemaining = 0;
        uint16_t channels = 0;
        uint16_t blockAlign = 0;
        // Mixer-owned while playing (reader sets them before playback starts).
        float currentDb = 0.0f;
        float startDb = 0.0f;
        float targetDb = 0.0f;
        uint32_t fadeFrames = 0;
        uint32_t fadePos = 0;
        bool stopAtEnd = false;
        // Reader -> mixer fade requests, guarded by controlLock.
        bool pending = false;
        float pendingDb = 0.0f;
        uint32_t pendingFrames = 0;
        bool pendingStop = false;
        float trim = 1.0f;                                   // mixer-owned once playing
    };

    Voice voices[VOICES];
    QueueHandle_t commands = nullptr;
    portMUX_TYPE controlLock = portMUX_INITIALIZER_UNLOCKED;
    bool ready = false;
    bool mounted = false;
    std::vector<Track> tracks;
    std::vector<char> names;

    const char *nameOf(const Track &track) const
    {
        return names.data() + track.nameOffset;
    }
    uint32_t indexGeneration = 0;
    int8_t volumeDb = 0;
    std::atomic<float> masterGain{1.0f};
    std::atomic<uint32_t> underruns{0};
    static constexpr size_t TRIMS = 4;
    static constexpr uint32_t TRIM_RAMP_MS = 1000;           // full 0 dB <-> silence swing
    static constexpr float TRIM_SLEW_PER_FRAME = 1000.0f / (TRIM_RAMP_MS * 44100.0f);
    std::atomic<uint16_t> trimTrack[TRIMS] = {};             // 0 = unused slot
    std::atomic<int32_t> trimQ15[TRIMS] = {};

    std::atomic<int32_t> effectsTrimQ15{32767};           // all effect tracks (below 500)

    float trimFor(uint16_t track) const
    {
        float gain = track < 500 ? effectsTrimQ15.load() / 32767.0f : 1.0f;
        for (size_t i = 0; i < TRIMS; ++i)
            if (trimTrack[i].load() == track)
                return gain * trimQ15[i].load() / 32767.0f;
        return gain;
    }
    uint32_t reportedUnderruns = 0;
    uint32_t lastUnderrunReport = 0;
    int16_t scratch[MAX_MIX_FRAMES];                       // mixer-only
    int16_t raw[CHUNK_BYTES / sizeof(int16_t)];            // reader-only
    int16_t mono[CHUNK_BYTES / sizeof(int16_t)];           // reader-only

    static int8_t clampDb(int db)
    {
        return static_cast<int8_t>(constrain(db, MIN_DB, 0));
    }

    static float dbToGain(float db)
    {
        return db <= MIN_DB ? 0.0f : powf(10.0f, db / 20.0f);
    }

    static uint32_t msToFrames(uint16_t ms)
    {
        return static_cast<uint32_t>(ms) * SAMPLE_RATE / 1000;
    }

    void setVolumeDb(int db)
    {
        volumeDb = static_cast<int8_t>(constrain(db, -40, 0));
        masterGain.store(powf(10.0f, volumeDb / 20.0f));
    }

    void send(const Command &command)
    {
        if (xQueueSend(commands, &command, 0) != pdTRUE)
            Serial.println("SD audio: command queue full; command dropped");
    }

    void applyPendingControl(Voice &voice)
    {
        portENTER_CRITICAL(&controlLock);
        const bool pending = voice.pending;
        if (pending)
        {
            voice.pending = false;
            voice.startDb = voice.currentDb;
            voice.targetDb = voice.pendingDb;
            voice.fadeFrames = voice.pendingFrames;
            voice.fadePos = 0;
            voice.stopAtEnd = voice.stopAtEnd || voice.pendingStop;
        }
        portEXIT_CRITICAL(&controlLock);
        if (pending && voice.fadeFrames == 0)
            voice.currentDb = voice.targetDb;
    }

    void release(Voice &voice, bool natural)
    {
        voice.naturalEnd = natural;
        voice.state.store(VOICE_RELEASING);
    }

    bool mount()
    {
        if (mounted)
            return true;
        SPI.begin(config::SD_SCK_PIN, config::SD_MISO_PIN, config::SD_MOSI_PIN, config::SD_CS_PIN);
        if (!SD.begin(config::SD_CS_PIN, SPI, config::SD_SPI_HZ, "/sd", VOICES))
        {
            SD.end();
            Serial.println("SD audio: card mount FAILED (check card, FAT32 format and wiring); d=retry");
            return false;
        }
        mounted = true;
        const uint8_t type = SD.cardType();
        const char *typeName = type == CARD_SDHC ? "SDHC" : type == CARD_SD ? "SD" :
                               type == CARD_MMC ? "MMC" : "unknown";
        Serial.printf("SD audio: %s card mounted, %llu MB\n", typeName,
                      static_cast<unsigned long long>(SD.cardSize() / (1024ULL * 1024ULL)));
        return true;
    }

    static bool parseTrackName(const char *name, uint16_t &number)
    {
        const size_t length = strlen(name);
        if (length < 8 || name[3] != '_' || strcasecmp(name + length - 4, ".wav") != 0)
            return false;
        for (uint8_t i = 0; i < 3; ++i)
            if (!isdigit(static_cast<unsigned char>(name[i])))
                return false;
        number = static_cast<uint16_t>((name[0] - '0') * 100 + (name[1] - '0') * 10 + (name[2] - '0'));
        return number > 0;
    }

    void buildIndex(bool printAll)
    {
        const uint32_t started = millis();
        tracks.clear();
        names.clear();
        DIR *dir = opendir("/sd");
        if (dir == nullptr)
        {
            Serial.println("SD audio: cannot open card root directory");
            return;
        }
        uint16_t ignored = 0;
        for (dirent *entry = readdir(dir); entry != nullptr; entry = readdir(dir))
        {
            uint16_t number = 0;
            if (entry->d_type == DT_DIR || !parseTrackName(entry->d_name, number))
            {
                ++ignored;
                continue;
            }
            const size_t length = strlen(entry->d_name) + 1;
            if (names.size() + length > 0xFFFF)
            {
                ++ignored;
                continue;
            }
            tracks.push_back({number, static_cast<uint16_t>(names.size())});
            names.insert(names.end(), entry->d_name, entry->d_name + length);
        }
        closedir(dir);
        tracks.shrink_to_fit();
        names.shrink_to_fit();
        std::sort(tracks.begin(), tracks.end(),
                  [](const Track &a, const Track &b) { return a.number < b.number; });

        uint16_t effects = 0;
        uint16_t music = 0;
        for (const Track &t : tracks)
            (t.number < 500 ? effects : music)++;
        Serial.printf("SD audio: %u numbered WAVs (%u effects 001-499, %u music 500+), %u other entries ignored, %lu ms\n",
                      static_cast<unsigned>(tracks.size()), static_cast<unsigned>(effects),
                      static_cast<unsigned>(music), static_cast<unsigned>(ignored),
                      static_cast<unsigned long>(millis() - started));

        uint8_t problems = 0;
        for (size_t i = 1; i < tracks.size() && problems < 8; ++i)
        {
            const uint16_t previous = tracks[i - 1].number;
            const uint16_t current = tracks[i].number;
            if (current == previous)
            {
                Serial.printf("SD audio: duplicate track %03u (%s)\n", static_cast<unsigned>(current),
                              nameOf(tracks[i]));
                ++problems;
            }
            else if (current != previous + 1 && !(previous < 500 && current >= 500))
            {
                Serial.printf("SD audio: gap after track %03u (next is %03u)\n",
                              static_cast<unsigned>(previous), static_cast<unsigned>(current));
                ++problems;
            }
        }

        ++indexGeneration;
        if (printAll)
            for (const Track &t : tracks)
                Serial.printf("  %03u %s\n", static_cast<unsigned>(t.number), nameOf(t));
    }

    static void readerEntry(void *context)
    {
        static_cast<PackSdAudio *>(context)->reader();
    }

    bool anyVoiceActive() const
    {
        for (const Voice &voice : voices)
            if (voice.state.load() != VOICE_FREE)
                return true;
        return false;
    }

    void closeFile(Voice &voice)
    {
        if (voice.file >= 0)
            close(voice.file);
        voice.file = -1;
    }

    // Reader-only: called for voices the mixer is not touching (Loading/Releasing).
    void reclaim(Voice &voice)
    {
        closeFile(voice);
        xStreamBufferReset(voice.ring);
        voice.fileDone.store(true);
        if (voice.naturalEnd && !voice.loop)
            Serial.printf("SD audio: track %03u finished\n", static_cast<unsigned>(voice.track));
        voice.naturalEnd = false;
        voice.track = 0;
        voice.state.store(VOICE_FREE);
    }

    void reader()
    {
        for (;;)
        {
            // Opening a file scans the card's large root directory, so start at most
            // one new track per pass and refill playing voices in between.
            Command command;
            TickType_t wait = anyVoiceActive() ? 0 : pdMS_TO_TICKS(20);
            bool startedOne = false;
            while (xQueueReceive(commands, &command, wait) == pdTRUE)
            {
                wait = 0;
                if (command.type == CommandType::Play)
                {
                    if (startedOne)
                    {
                        xQueueSendToFront(commands, &command, 0);
                        break;
                    }
                    startedOne = true;
                }
                handle(command);
            }
            if (!serviceVoices())
                vTaskDelay(pdMS_TO_TICKS(2));
        }
    }

    // Reclaims finished voices and refills every playing or loading voice once.
    bool serviceVoices()
    {
        bool worked = false;
        for (Voice &voice : voices)
        {
            const uint8_t state = voice.state.load();
            if (state == VOICE_RELEASING)
            {
                reclaim(voice);
                continue;
            }
            if (state != VOICE_LOADING && state != VOICE_PLAYING)
                continue;
            if (voice.file >= 0 && fill(voice))
                worked = true;
            if (state == VOICE_LOADING &&
                (voice.fileDone.load() || xStreamBufferBytesAvailable(voice.ring) >= PREFILL_BYTES))
                voice.state.store(VOICE_PLAYING);
        }
        return worked;
    }

    void handle(const Command &command)
    {
        if (command.type == CommandType::Play)
            startVoice(command);
        else if (command.type == CommandType::Fade)
            fadeVoices(command);
        else
            for (Voice &voice : voices)
            {
                const uint8_t state = voice.state.load();
                if ((state == VOICE_LOADING || state == VOICE_PLAYING) && voice.track == command.track)
                    voice.loop = false;
            }
    }

    Voice *freeVoice()
    {
        for (Voice &voice : voices)
            if (voice.state.load() == VOICE_FREE)
                return &voice;
        for (Voice &voice : voices)
            if (voice.state.load() == VOICE_RELEASING)
            {
                reclaim(voice);
                return &voice;
            }
        return nullptr;
    }

    void startVoice(const Command &command)
    {
        Voice *voice = freeVoice();
        // Stops queued just before this play finish within a few mixer blocks.
        for (uint8_t waited = 0; voice == nullptr && waited < 20; ++waited)
        {
            if (!serviceVoices())
                vTaskDelay(pdMS_TO_TICKS(1));
            voice = freeVoice();
        }
        if (voice == nullptr)
        {
            Serial.printf("SD audio: no free voice for track %03u\n", static_cast<unsigned>(command.track));
            return;
        }
        if (!openTrack(*voice, command))
            return;
        xStreamBufferReset(voice->ring);
        voice->track = command.track;
        voice->loop = command.loop;
        voice->naturalEnd = false;
        voice->pending = false;
        voice->stopAtEnd = false;
        voice->fadePos = 0;
        voice->targetDb = command.gainDb;
        if (command.ms > 0)
        {
            voice->currentDb = MIN_DB;
            voice->startDb = MIN_DB;
            voice->fadeFrames = msToFrames(command.ms);
        }
        else
        {
            voice->currentDb = command.gainDb;
            voice->startDb = command.gainDb;
            voice->fadeFrames = 0;
        }
        voice->trim = trimFor(command.track);
        voice->fileDone.store(false);
        voice->state.store(VOICE_LOADING);
    }

    void fadeVoices(const Command &command)
    {
        for (Voice &voice : voices)
        {
            const uint8_t state = voice.state.load();
            if ((state != VOICE_LOADING && state != VOICE_PLAYING) ||
                (command.track != 0 && voice.track != command.track))
                continue;
            if (state == VOICE_LOADING)
            {
                // Not audible yet, so the mixer is not using it.
                if (command.stopAtEnd)
                    reclaim(voice);
                else
                {
                    voice.startDb = voice.currentDb;
                    voice.targetDb = command.gainDb;
                    voice.fadeFrames = msToFrames(command.ms);
                    voice.fadePos = 0;
                    if (voice.fadeFrames == 0)
                        voice.currentDb = voice.targetDb;
                }
                continue;
            }
            portENTER_CRITICAL(&controlLock);
            voice.pending = true;
            voice.pendingDb = command.gainDb;
            voice.pendingFrames = msToFrames(command.ms);
            voice.pendingStop = command.stopAtEnd;
            portEXIT_CRITICAL(&controlLock);
        }
    }

    // Reads one chunk if the voice's buffer has room; loops or finishes at the end.
    bool fill(Voice &voice)
    {
        // Each frame becomes one mono sample in the ring, whatever the file's channels.
        const size_t freeFrames = xStreamBufferSpacesAvailable(voice.ring) / sizeof(int16_t);
        if (freeFrames < MIN_FILL_FRAMES)
            return false;
        size_t wanted = freeFrames * voice.blockAlign;
        if (wanted > CHUNK_BYTES)
            wanted = CHUNK_BYTES;
        if (wanted > voice.dataRemaining)
            wanted = voice.dataRemaining;
        const ssize_t result = read(voice.file, raw, wanted);
        const bool readError = result < 0;
        const size_t got = readError ? 0 : static_cast<size_t>(result) - static_cast<size_t>(result) % voice.blockAlign;
        const size_t frames = got / voice.blockAlign;
        for (size_t i = 0; i < frames; ++i)
            mono[i] = voice.channels == 2 ?
                      static_cast<int16_t>((static_cast<int32_t>(raw[i * 2]) + raw[i * 2 + 1]) >> 1) :
                      raw[i];
        xStreamBufferSend(voice.ring, mono, frames * sizeof(int16_t), 0);
        voice.dataRemaining -= got;

        if (voice.dataRemaining == 0 || got < wanted)
        {
            const bool shortRead = got < wanted;
            if (readError)
                Serial.printf("SD audio: read error on track %03u\n", static_cast<unsigned>(voice.track));
            if (voice.loop && !shortRead && lseek(voice.file, voice.dataStart, SEEK_SET) >= 0)
                voice.dataRemaining = voice.dataBytes;
            else
            {
                closeFile(voice);
                voice.fileDone.store(true);
            }
        }
        return got > 0;
    }

    static uint16_t le16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
    static uint32_t le32(const uint8_t *p)
    {
        return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    }

    bool rejectTrack(Voice &voice, const Command &command, const char *reason)
    {
        Serial.printf("SD audio: cannot play %03u: %s\n", static_cast<unsigned>(command.track), reason);
        closeFile(voice);
        return false;
    }

    // Parses RIFF/WAVE chunks, skipping metadata, and leaves the file at the PCM data.
    bool openTrack(Voice &voice, const Command &command)
    {
        voice.file = open(command.path, O_RDONLY);
        if (voice.file < 0)
            return rejectTrack(voice, command, "open failed");

        uint8_t header[12];
        if (read(voice.file, header, sizeof(header)) != static_cast<ssize_t>(sizeof(header)) ||
            memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0)
            return rejectTrack(voice, command, "not a RIFF/WAVE file");

        bool haveFormat = false;
        uint16_t format = 0;
        uint16_t bits = 0;
        uint32_t rate = 0;
        uint32_t dataSize = 0;
        for (;;)
        {
            uint8_t chunk[8];
            if (read(voice.file, chunk, sizeof(chunk)) != static_cast<ssize_t>(sizeof(chunk)))
                return rejectTrack(voice, command, "no data chunk");
            const uint32_t size = le32(chunk + 4);
            if (memcmp(chunk, "fmt ", 4) == 0)
            {
                uint8_t fmt[40];
                const size_t length = std::min<size_t>(size, sizeof(fmt));
                if (size < 16 || read(voice.file, fmt, length) != static_cast<ssize_t>(length))
                    return rejectTrack(voice, command, "bad fmt chunk");
                format = le16(fmt);
                voice.channels = le16(fmt + 2);
                rate = le32(fmt + 4);
                voice.blockAlign = le16(fmt + 12);
                bits = le16(fmt + 14);
                if (format == 0xFFFE && length >= 26)
                    format = le16(fmt + 24);
                const uint32_t skip = (size - length) + (size & 1);
                if (skip != 0 && lseek(voice.file, skip, SEEK_CUR) < 0)
                    return rejectTrack(voice, command, "bad fmt chunk");
                haveFormat = true;
            }
            else if (memcmp(chunk, "data", 4) == 0)
            {
                if (!haveFormat)
                    return rejectTrack(voice, command, "data before fmt chunk");
                dataSize = size;
                break;
            }
            else if (lseek(voice.file, size + (size & 1), SEEK_CUR) < 0)
                return rejectTrack(voice, command, "truncated chunk");
        }

        if (format != 1 || bits != 16 || rate != SAMPLE_RATE ||
            (voice.channels != 1 && voice.channels != 2) || voice.blockAlign != voice.channels * 2)
        {
            Serial.printf("SD audio: format is %u ch, %lu Hz, %u-bit, type %u\n",
                          static_cast<unsigned>(voice.channels), static_cast<unsigned long>(rate),
                          static_cast<unsigned>(bits), static_cast<unsigned>(format));
            return rejectTrack(voice, command, "needs 16-bit PCM, 44.1 kHz, mono or stereo");
        }
        const off_t start = lseek(voice.file, 0, SEEK_CUR);
        voice.dataBytes = dataSize - dataSize % voice.blockAlign;
        if (start < 0 || voice.dataBytes == 0)
            return rejectTrack(voice, command, "empty or unreadable audio data");
        voice.dataStart = static_cast<uint32_t>(start);
        voice.dataRemaining = voice.dataBytes;
        Serial.printf("SD audio: play %03u, %s, %.1f s%s\n", static_cast<unsigned>(command.track),
                      voice.channels == 2 ? "stereo" : "mono",
                      static_cast<double>(voice.dataBytes / voice.blockAlign) / SAMPLE_RATE,
                      command.loop ? ", looping" : "");
        return true;
    }
};
