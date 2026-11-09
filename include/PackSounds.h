#pragma once

#include "Config.h"
#include "PackController.h"
#include "PackSdAudio.h"
#include "WandPackLink.h"

// GPStar sound_fx enum indexes; each is the NNN_ prefix of a WAV on the SD card.
namespace sfx {
constexpr uint16_t BOOTUP = 1;                         // S_BOOTUP
constexpr uint16_t FIRE_LOOP_IMPACT = 9;               // S_FIRE_LOOP_IMPACT
constexpr uint16_t FIRING_END = 10;                    // S_FIRING_END
constexpr uint16_t FIRING_END_GUN = 11;                // S_FIRING_END_GUN
constexpr uint16_t AFTERLIFE_PACK_STARTUP = 15;        // S_AFTERLIFE_PACK_STARTUP
constexpr uint16_t AFTERLIFE_PACK_IDLE_LOOP = 16;      // S_AFTERLIFE_PACK_IDLE_LOOP
constexpr uint16_t WAND_SHUTDOWN = 17;                 // S_WAND_SHUTDOWN
constexpr uint16_t AFTERLIFE_BEEP_WAND_S5 = 22;        // S_AFTERLIFE_BEEP_WAND_S5
constexpr uint16_t PACK_SHUTDOWN = 35;                 // S_PACK_SHUTDOWN
constexpr uint16_t GB2_PACK_START = 37;                // S_GB2_PACK_START
constexpr uint16_t GB2_PACK_LOOP = 38;                 // S_GB2_PACK_LOOP
constexpr uint16_t GB2_PACK_OFF = 39;                  // S_GB2_PACK_OFF
constexpr uint16_t BEEP_8 = 60;                        // S_BEEP_8
constexpr uint16_t VENT_SMOKE = 61;                    // S_VENT_SMOKE
constexpr uint16_t FIRING_END_MID = 71;                // S_FIRING_END_MID
constexpr uint16_t FIRING_LOOP_GB1 = 72;               // S_FIRING_LOOP_GB1
constexpr uint16_t GB1_FIRE_START_HIGH_POWER = 97;     // S_GB1_FIRE_START_HIGH_POWER
constexpr uint16_t GB1_FIRE_HIGH_POWER_LOOP = 98;      // S_GB1_FIRE_HIGH_POWER_LOOP
constexpr uint16_t WAND_RAMP_1 = 141;                  // S_AFTERLIFE_WAND_RAMP_1
constexpr uint16_t WAND_IDLE_1 = 142;                  // S_AFTERLIFE_WAND_IDLE_1
constexpr uint16_t WAND_RAMP_2 = 143;                  // S_AFTERLIFE_WAND_RAMP_2
constexpr uint16_t WAND_IDLE_2 = 144;                  // S_AFTERLIFE_WAND_IDLE_2
constexpr uint16_t WAND_RAMP_DOWN_1 = 145;             // S_AFTERLIFE_WAND_RAMP_DOWN_1
constexpr uint16_t WAND_RAMP_DOWN_2 = 146;             // S_AFTERLIFE_WAND_RAMP_DOWN_2
constexpr uint16_t WAND_RAMP_2_FADE_IN = 147;          // S_AFTERLIFE_WAND_RAMP_2_FADE_IN
constexpr uint16_t WAND_RAMP_DOWN_2_FADE_OUT = 148;    // S_AFTERLIFE_WAND_RAMP_DOWN_2_FADE_OUT
constexpr uint16_t AFTERLIFE_FIRE_START = 155;         // S_AFTERLIFE_FIRE_START
constexpr uint16_t AIR_RELEASE = 178;                  // S_AIR_RELEASE
constexpr uint16_t STEAM_LOOP = 179;                   // S_STEAM_LOOP
constexpr uint16_t STEAM_LOOP_FADE_OUT = 180;          // S_STEAM_LOOP_FADE_OUT
constexpr uint16_t WAND_BOOTUP = 329;                  // S_WAND_BOOTUP
constexpr uint16_t GB2_WAND_START = 333;               // S_GB2_WAND_START
constexpr uint16_t FROZEN_EMPIRE_FIRE_START = 335;     // S_FROZEN_EMPIRE_FIRE_START
constexpr uint16_t VENT_OPEN = 341;                    // S_VENT_OPEN
constexpr uint16_t VENT_CLOSE = 342;                   // S_VENT_CLOSE
constexpr uint16_t PACK_SHUTDOWN_AFTERLIFE_ALT = 348;  // S_PACK_SHUTDOWN_AFTERLIFE_ALT
constexpr uint16_t VENT_CLOSE_ALT = 355;               // S_VENT_CLOSE_ALT
constexpr uint16_t VENT_SMOKE_1 = 356;                 // S_VENT_SMOKE_1 (_2.._4 follow)
constexpr uint16_t PACK_PRE_VENT = 360;                // S_PACK_PRE_VENT
constexpr uint16_t PACK_OVERHEAT_HOT = 361;            // S_PACK_OVERHEAT_HOT
constexpr uint16_t PACK_BEEPS_OVERHEAT = 362;          // S_PACK_BEEPS_OVERHEAT
constexpr uint16_t AFTERLIFE_FIRE_END_SHORT = 385;     // S_AFTERLIFE_FIRE_END_SHORT
constexpr uint16_t AFTERLIFE_FIRE_END_MID = 386;       // S_AFTERLIFE_FIRE_END_MID
constexpr uint16_t AFTERLIFE_FIRE_END_LONG = 387;      // S_AFTERLIFE_FIRE_END_LONG
constexpr uint16_t GB1_1984_BOOT_UP = 379;             // S_GB1_1984_BOOT_UP
constexpr uint16_t GB1_1984_PACK_LOOP = 380;           // S_GB1_1984_PACK_LOOP
constexpr uint16_t GB1_1984_FIRE_START_HIGH_POWER = 391;      // S_GB1_1984_FIRE_START_HIGH_POWER
constexpr uint16_t GB1_1984_FIRE_END_HIGH_POWER = 392;        // S_GB1_1984_FIRE_END_HIGH_POWER
constexpr uint16_t GB1_1984_FIRE_HIGH_POWER_LOOP = 393;       // S_GB1_1984_FIRE_HIGH_POWER_LOOP
constexpr uint16_t GB1_1984_FIRE_END_SHORT_HIGH_POWER = 394;  // S_GB1_1984_FIRE_END_SHORT_HIGH_POWER
constexpr uint16_t GB1_1984_FIRE_END_MID_HIGH_POWER = 395;    // S_GB1_1984_FIRE_END_MID_HIGH_POWER
constexpr uint16_t FROZEN_EMPIRE_PACK_IDLE_LOOP = 405; // S_FROZEN_EMPIRE_PACK_IDLE_LOOP
constexpr uint16_t FROZEN_EMPIRE_PACK_STARTUP = 413;   // S_FROZEN_EMPIRE_PACK_STARTUP
constexpr uint16_t FROZEN_EMPIRE_PACK_SHUTDOWN = 418;  // S_FROZEN_EMPIRE_PACK_SHUTDOWN
}

// Pack sounds per GPStar system theme (proton stream, power level 5, intensify).
struct ThemeSounds
{
    uint16_t startup;          // packStartup(true)
    uint16_t quickStartup;     // packStartup(false)
    uint16_t idleLoop;
    uint16_t idleFadeMs;       // idle loop fade-in after startup
    uint16_t quickIdleFadeMs;
    uint16_t shutdown;
    uint16_t fireStart;
    uint16_t fireLoop;
    uint16_t fireLoopFadeMs;
    uint16_t tailShort;        // tails by firing length on the 15 s timer
    uint16_t tailMid;
    uint16_t tailLong;
    uint16_t warningBeep;      // W_BEEP_START
    uint16_t ventClose;        // packOverheatingFinished()
};

inline ThemeSounds themeSounds(SystemTheme theme)
{
    using namespace sfx;
    switch (theme)
    {
    case SystemTheme::Gb1984:
        return {GB1_1984_BOOT_UP, GB1_1984_BOOT_UP, GB1_1984_PACK_LOOP, 3800, 3800, PACK_SHUTDOWN,
                GB1_1984_FIRE_START_HIGH_POWER, GB1_1984_FIRE_HIGH_POWER_LOOP, 1700,
                GB1_1984_FIRE_END_SHORT_HIGH_POWER, GB1_1984_FIRE_END_MID_HIGH_POWER,
                GB1_1984_FIRE_END_HIGH_POWER, BEEP_8, VENT_CLOSE};
    case SystemTheme::Gb1989:
        return {GB2_PACK_START, GB2_PACK_START, GB2_PACK_LOOP, 3800, 3800, GB2_PACK_OFF,
                GB1_FIRE_START_HIGH_POWER, GB1_FIRE_HIGH_POWER_LOOP, 700,
                FIRING_END, FIRING_END_MID, FIRING_END_GUN, BEEP_8, VENT_CLOSE};
    case SystemTheme::FrozenEmpire:
        // Frozen Empire always plays the mid tail ("thump").
        return {FROZEN_EMPIRE_PACK_STARTUP, BOOTUP, FROZEN_EMPIRE_PACK_IDLE_LOOP, 10000, 500,
                FROZEN_EMPIRE_PACK_SHUTDOWN, FROZEN_EMPIRE_FIRE_START, GB1_FIRE_HIGH_POWER_LOOP, 300,
                AFTERLIFE_FIRE_END_MID, AFTERLIFE_FIRE_END_MID, AFTERLIFE_FIRE_END_MID,
                PACK_BEEPS_OVERHEAT, VENT_CLOSE_ALT};
    case SystemTheme::Afterlife:
    default:
        return {AFTERLIFE_PACK_STARTUP, BOOTUP, AFTERLIFE_PACK_IDLE_LOOP, 15000, 500,
                PACK_SHUTDOWN_AFTERLIFE_ALT, AFTERLIFE_FIRE_START, GB1_FIRE_HIGH_POWER_LOOP, 300,
                AFTERLIFE_FIRE_END_SHORT, AFTERLIFE_FIRE_END_MID, AFTERLIFE_FIRE_END_LONG,
                PACK_BEEPS_OVERHEAT, VENT_CLOSE_ALT};
    }
}

// GPStar system themes (1984, 1989, Afterlife, Frozen Empire), Super Hero mode,
// proton stream at power level 5 (the GPStar Super Hero default), as heard from a
// pack with extra wand sounds enabled. Per-theme pack tracks are in themeSounds();
// 1984/1989 wand sounds follow GPStar's 80s branches (boot on Vent, shutdown on
// Vent off, one beep per arm). Afterlife and Frozen Empire share the wand logic below.
// Pack sounds follow ProtonPack packStartup()/packShutdown()/modeFireStartSounds()/
// modeFireStopSounds() and the overheat sequence (W_BEEP_START warning beeps,
// packOverheatingStart(), cyclotronOverheating(), packOverheatingFinished()); our
// pack/wand Cooling state is GPStar's overheat. Wand sounds follow the NeutronaWand activation, vent
// (wandVentStateCheck), beep-loop and wandOff() logic, played as the pack does
// for GPStar's W_* extra-sound commands at WAND_SOUND_LEVEL_DB. Tracks that both
// GPStar boards would play (firing) are played once.
class PackSounds
{
public:
    void update(const PackController &pack, uint8_t wandFlags, uint8_t speedLevel,
                PackSdAudio &sd, uint32_t now)
    {
        if (pack.theme() != activeTheme)
            changeTheme(pack, sd);
        updatePack(pack, speedLevel, sd, now);
        if (config::themeIsModern(activeTheme))
            updateWand(wandFlags, sd, now);
        else
            updateWand80s(wandFlags, sd);
    }

private:
    static constexpr uint32_t GUN_RAMP_1_MS = 1768;   // GPStar i_gun_ramp_1_length (track 141)
    static constexpr uint32_t GUN_RAMP_2_MS = 1881;   // GPStar i_gun_ramp_2_length (track 143)
    static constexpr uint32_t BEEP_RESTART_MS = 1000; // GPStar i_sound_timer
    static constexpr uint32_t FIRING_TIMER_MS = 15000; // GPStar i_firing_timer_length

    SystemTheme activeTheme = config::SYSTEM_THEME;
    PackState lastState = PackState::Off;
    uint32_t firingStartedMs = 0;
    bool lastArm = false;
    bool overheatBeeping = false;
    bool steamStarted = false;
    uint32_t overheatStartedMs = 0;

    bool wandOn = false;
    bool wandOverheating = false;
    bool soundIdle = false;        // GPStar b_sound_idle: vent-on (ramp 2 / idle 2) sounds active
    bool idle2Fade = true;         // GPStar b_sound_afterlife_idle_2_fade
    bool beeping = false;
    bool ramp1Running = false;
    bool ramp2Running = false;
    uint32_t ramp1EndsMs = 0;
    uint32_t ramp2EndsMs = 0;
    uint32_t beepRestartMs = 0;

    static int8_t wandDb() { return config::WAND_SOUND_LEVEL_DB; }
    static int8_t readyDb() { return config::WAND_READY_LEVEL_DB; }

    ThemeSounds tracks() const { return themeSounds(activeTheme); }

    // Dip the pack startup/idle sounds while the wand is ready (Vent on).
    void packReadyTrim(PackSdAudio &sd, bool ready) const
    {
        const int db = ready ? config::PACK_READY_TRIM_DB : 0;
        sd.setTrim(tracks().startup, db);
        sd.setTrim(tracks().idleLoop, db);
    }

    // Switching themes while the pack is on crossfades to the new theme's idle
    // loop; wand sounds restart from the switch positions on the next update.
    void changeTheme(const PackController &pack, PackSdAudio &sd)
    {
        const ThemeSounds old = tracks();
        packReadyTrim(sd, false);
        stopWandSounds(sd);
        sd.stop(sfx::AFTERLIFE_BEEP_WAND_S5);
        activeTheme = pack.theme();
        wandOn = false;
        wandOverheating = false;
        soundIdle = false;
        beeping = false;
        ramp1Running = false;
        ramp2Running = false;
        const PackState state = pack.state();
        if (state == PackState::Off || state == PackState::Stopping)
            return;
        sd.stop(old.startup);
        sd.fadeout(old.idleLoop, 500);
        sd.play(tracks().idleLoop, true, 0, 500);
    }

    void updatePack(const PackController &pack, uint8_t speedLevel, PackSdAudio &sd, uint32_t now)
    {
        const PackState state = pack.state();
        if (state != lastState)
        {
            // packShutdown() stops a firing wand without tail sounds.
            if (lastState == PackState::Firing)
                fireStop(sd, now, state != PackState::Stopping);
            if (lastState == PackState::Cooling)
                overheatFinish(sd, state != PackState::Stopping);
            if (state == PackState::Starting)
                packStartup(sd);
            else if (state == PackState::Stopping)
                packShutdown(sd);
            else if (state == PackState::Firing)
                fireStart(sd, now);
            else if (state == PackState::Cooling)
                overheatStart(sd, now);
            lastState = state;
        }

        // cyclotronSpeedUp(4): warning beeps once a quarter of the overheat timer
        // remains (our speed level 4).
        if (state == PackState::Firing && speedLevel >= 4 && !overheatBeeping)
        {
            sd.play(tracks().warningBeep, true, config::OVERHEAT_BEEP_LEVEL_DB);
            overheatBeeping = true;
        }
        // cyclotronOverheating(): steam after i_overheating_delay.
        if (state == PackState::Cooling && !steamStarted &&
            now - overheatStartedMs >= config::OVERHEAT_STEAM_DELAY_MS)
        {
            steamStarted = true;
            // Simplified by request: no 178 air release.
            const long burst = random(5);
            sd.play(burst == 0 ? sfx::VENT_SMOKE : static_cast<uint16_t>(sfx::VENT_SMOKE_1 + burst - 1),
                    false, config::OVERHEAT_SOUND_LEVEL_DB, 120);
            sd.play(sfx::STEAM_LOOP, true, config::OVERHEAT_SOUND_LEVEL_DB, 1000);
        }
    }

    // packOverheatingStart().
    void overheatStart(PackSdAudio &sd, uint32_t now)
    {
        overheatStartedMs = now;
        steamStarted = false;
        // Simplified by request: GPStar also plays 360 pre-vent and the 361 overheat
        // hot loop here; opening several files at once glitched the audio.
        sd.play(sfx::VENT_OPEN, false, config::OVERHEAT_SOUND_LEVEL_DB);
    }

    // packOverheatingFinished() when the cooldown completes; a shutdown during the
    // overheat just stops the overheat sounds.
    void overheatFinish(PackSdAudio &sd, bool completed) const
    {
        sd.stop(sfx::STEAM_LOOP);
        sd.stop(sfx::PACK_OVERHEAT_HOT);
        if (completed)
        {
            sd.play(tracks().ventClose, false, config::OVERHEAT_SOUND_LEVEL_DB);
            sd.play(sfx::STEAM_LOOP_FADE_OUT, false, config::OVERHEAT_SOUND_LEVEL_DB);
        }
    }

    void packStartup(PackSdAudio &sd) const
    {
        const ThemeSounds t = tracks();
        // Long shutdown tracks may still be playing from a quick restart.
        sd.stop(sfx::PACK_SHUTDOWN);
        sd.stop(sfx::GB2_PACK_OFF);
        sd.stop(sfx::PACK_SHUTDOWN_AFTERLIFE_ALT);
        sd.stop(sfx::FROZEN_EMPIRE_PACK_SHUTDOWN);
        sd.stop(t.idleLoop);
        if (config::AUDIO_LONG_STARTUP)
        {
            sd.stop(t.startup);
            sd.play(t.startup);
            sd.play(t.idleLoop, true, 0, t.idleFadeMs);
        }
        else
        {
            sd.stop(t.quickStartup);
            sd.play(t.quickStartup);
            sd.play(t.idleLoop, true, 0, t.quickIdleFadeMs);
        }
    }

    void packShutdown(PackSdAudio &sd) const
    {
        const ThemeSounds t = tracks();
        sd.play(t.shutdown);
        sd.stop(sfx::BOOTUP);
        sd.stop(sfx::AFTERLIFE_FIRE_END_LONG);
        sd.stop(t.startup);
        sd.fadeout(t.idleLoop);
    }

    // modeFireStartSounds(), proton stream, level 5, intensify (our fire button).
    void fireStart(PackSdAudio &sd, uint32_t now)
    {
        firingStartedMs = now;
        sd.play(tracks().fireStart);
        sd.play(tracks().fireLoop, true, 0, tracks().fireLoopFadeMs);
    }

    // modeFireStopSounds() tail chosen by firing length, then wandStopFiringSounds().
    void fireStop(PackSdAudio &sd, uint32_t now, bool tail)
    {
        if (tail)
        {
            const uint32_t fired = now - firingStartedMs;
            const uint32_t remaining = fired < FIRING_TIMER_MS ? FIRING_TIMER_MS - fired : 0;
            const ThemeSounds t = tracks();
            sd.play(remaining < 5000 ? t.tailLong : remaining < 10000 ? t.tailMid : t.tailShort);
        }
        sd.stop(tracks().fireStart);
        sd.stop(sfx::FIRE_LOOP_IMPACT);
        sd.stop(sfx::FIRING_LOOP_GB1);
        sd.stop(tracks().fireLoop);
        // stopOverheatBeepWarnings().
        sd.stop(tracks().warningBeep);
        overheatBeeping = false;
    }

    void updateWand(uint8_t flags, PackSdAudio &sd, uint32_t now)
    {
        const bool activate = (flags & wand_link::Activate) != 0;
        const bool vent = (flags & wand_link::Vent) != 0;
        const bool armSwitch = (flags & wand_link::Enabled) != 0;
        const bool firing = (flags & wand_link::Firing) != 0;
        const bool cooling = (flags & wand_link::Cooling) != 0;

        if (activate && !wandOn)
            wandActivate(sd, now, vent);
        else if (!activate && wandOn)
        {
            wandOff(sd);
            return;
        }
        if (!wandOn)
            return;

        // startVentSequence(): the wand's overheat silences its idle and beep sounds
        // until the pack reports the overheat finished (our Cooling flag clears).
        if (cooling)
        {
            if (!wandOverheating)
            {
                wandOverheating = true;
                beepLoopStop(sd, now, activate && vent && armSwitch);
                // Simplified by request: no 017 wand shutdown sound at overheat.
                soundIdleStop(sd, now, true);
            }
            return;
        }
        if (wandOverheating)
        {
            // overheatingFinished(): ramp 1 if the vent is off; otherwise the vent
            // check below restarts ramp 2.
            wandOverheating = false;
            if (!vent && config::WAND_IDLE_1_SOUNDS)
            {
                stopWandSounds(sd);
                sd.play(sfx::WAND_RAMP_1, false, wandDb());
                ramp1Running = true;
                ramp1EndsMs = now + GUN_RAMP_1_MS;
            }
        }

        // wandVentStateCheck(): vent on runs the ramp 2 / idle 2 sounds and, with
        // the arm switch on and not firing, the beep loop.
        const bool allOn = activate && vent && armSwitch;
        if (vent)
        {
            if (!soundIdle)
                soundIdleStart(sd, now);
            if (ramp2Running && static_cast<int32_t>(now - ramp2EndsMs) >= 0)
            {
                ramp2Running = false;
                sd.stop(sfx::WAND_IDLE_2);
                sd.play(sfx::WAND_IDLE_2, true, readyDb());
            }
            if (armSwitch && !firing)
                beepLoop(sd, now);
            else
                beepLoopStop(sd, now, allOn);
        }
        else
        {
            beepLoopStop(sd, now, allOn);
            soundIdleStop(sd, now, false);
            if (ramp1Running && static_cast<int32_t>(now - ramp1EndsMs) >= 0)
            {
                ramp1Running = false;
                sd.stop(sfx::WAND_IDLE_1);
                sd.play(sfx::WAND_IDLE_1, true, wandDb());
            }
        }
    }

    // modeActivate()/postActivation(): with the vent switch off, afterlifeRampSound1().
    void wandActivate(PackSdAudio &sd, uint32_t now, bool vent)
    {
        wandOn = true;
        soundIdle = false;
        beeping = false;
        ramp2Running = false;
        beepRestartMs = now;
        idle2Fade = true;
        if (!vent)
            idle2Fade = false;
        if (!vent && config::WAND_IDLE_1_SOUNDS)
        {
            stopWandSounds(sd);
            sd.play(sfx::WAND_RAMP_1, false, wandDb());
            ramp1Running = true;
            ramp1EndsMs = now + GUN_RAMP_1_MS;
        }
    }

    // wandOff(): wand shutdown sound, stop wand sounds, then the matching ramp-down.
    void wandOff(PackSdAudio &sd)
    {
        sd.play(sfx::WAND_SHUTDOWN);
        stopWandSounds(sd);
        sd.stop(sfx::AFTERLIFE_BEEP_WAND_S5);
        if (soundIdle)
            sd.play(sfx::WAND_RAMP_DOWN_2_FADE_OUT, false, readyDb());
        else if (config::WAND_IDLE_1_SOUNDS)
            sd.play(sfx::WAND_RAMP_DOWN_1, false, wandDb());
        packReadyTrim(sd, false);
        wandOn = false;
        wandOverheating = false;
        soundIdle = false;
        beeping = false;
        ramp1Running = false;
        ramp2Running = false;
        idle2Fade = true;
    }

    // soundIdleStart(): ramp 2 (fade-in variant if the vent was already on at
    // activation), then idle 2 loops after it.
    void soundIdleStart(PackSdAudio &sd, uint32_t now)
    {
        const uint16_t ramp = idle2Fade ? sfx::WAND_RAMP_2_FADE_IN : sfx::WAND_RAMP_2;
        idle2Fade = false;
        ramp1Running = false;
        ramp2Running = true;
        ramp2EndsMs = now + GUN_RAMP_2_MS;
        sd.stop(ramp);
        sd.play(ramp, false, readyDb());
        packReadyTrim(sd, true);
        sd.stop(sfx::WAND_RAMP_1);
        sd.stop(sfx::WAND_IDLE_1);
        sd.stop(sfx::WAND_RAMP_DOWN_1);
        sd.stop(sfx::WAND_RAMP_DOWN_2);
        sd.stop(sfx::WAND_RAMP_DOWN_2_FADE_OUT);
        soundIdle = true;
    }

    // soundIdleStop(): with the wand still on, ramp 2 down and then idle 1 loops;
    // when overheating, the fade-out ramp-down and nothing after it.
    void soundIdleStop(PackSdAudio &sd, uint32_t now, bool overheating)
    {
        if (!soundIdle)
            return;
        ramp2Running = false;
        if (overheating)
        {
            // Simplified by request: fade the idle 2 hum out instead of opening the
            // 148 ramp-down file.
            ramp1Running = false;
            sd.fadeout(sfx::WAND_IDLE_2, 500);
        }
        else
        {
            ramp1Running = config::WAND_IDLE_1_SOUNDS;
            ramp1EndsMs = now + GUN_RAMP_2_MS;
            sd.stop(sfx::WAND_RAMP_DOWN_2);
            sd.play(sfx::WAND_RAMP_DOWN_2, false, readyDb());
            sd.stop(sfx::WAND_IDLE_2);
        }
        packReadyTrim(sd, false);
        sd.stop(sfx::WAND_RAMP_2);
        sd.stop(sfx::WAND_RAMP_2_FADE_IN);
        soundIdle = false;
    }

    // soundBeepLoop(): level 5 Afterlife beep loop once the restart timer has run.
    void beepLoop(PackSdAudio &sd, uint32_t now)
    {
        if (beeping || static_cast<int32_t>(now - beepRestartMs) < 0)
            return;
        sd.stop(sfx::AFTERLIFE_BEEP_WAND_S5);
        sd.play(sfx::AFTERLIFE_BEEP_WAND_S5, true, config::WAND_BEEP_LEVEL_DB);
        beeping = true;
    }

    // soundBeepLoopStop(): with every switch on (e.g. while firing) the loop ends
    // naturally and restarts 1 s after the last stop; otherwise it stops at once.
    void beepLoopStop(PackSdAudio &sd, uint32_t now, bool allOn)
    {
        if (allOn)
            beepRestartMs = now + BEEP_RESTART_MS;
        if (!beeping)
            return;
        beeping = false;
        if (allOn)
            sd.endLoop(sfx::AFTERLIFE_BEEP_WAND_S5);
        else
        {
            sd.stop(sfx::AFTERLIFE_BEEP_WAND_S5);
            beepRestartMs = now;
        }
    }

    // 1984/1989 wand (GPStar soundIdleStart/soundIdleStop/soundBeepLoop 80s paths):
    // turning the Vent on plays the wand boot (329, or 333 for 1989), turning it off
    // plays 017, and switching the arm on plays one 022 beep. Activation and
    // overheat recovery sounds are wand-local in GPStar, so the pack stays silent.
    void updateWand80s(uint8_t flags, PackSdAudio &sd)
    {
        const bool activate = (flags & wand_link::Activate) != 0;
        const bool vent = (flags & wand_link::Vent) != 0;
        const bool armSwitch = (flags & wand_link::Enabled) != 0;
        const bool firing = (flags & wand_link::Firing) != 0;
        const bool cooling = (flags & wand_link::Cooling) != 0;

        if (activate && !wandOn)
        {
            wandOn = true;
            wandOverheating = false;
            soundIdle = vent;
            lastArm = armSwitch;
            if (vent)
                packReadyTrim(sd, true);
            return;
        }
        if (!activate && wandOn)
        {
            if (soundIdle)
                sd.play(sfx::WAND_SHUTDOWN);
            packReadyTrim(sd, false);
            wandOn = false;
            soundIdle = false;
            return;
        }
        if (!wandOn)
            return;
        if (cooling)
        {
            wandOverheating = true;
            lastArm = armSwitch;
            return;
        }
        if (wandOverheating)
        {
            wandOverheating = false;
            soundIdle = vent;
        }

        if (vent && !soundIdle)
        {
            const uint16_t boot = activeTheme == SystemTheme::Gb1989 ? sfx::GB2_WAND_START : sfx::WAND_BOOTUP;
            sd.stop(boot);
            sd.play(boot, false, readyDb());
            packReadyTrim(sd, true);
            soundIdle = true;
        }
        else if (!vent && soundIdle)
        {
            sd.play(sfx::WAND_SHUTDOWN);
            packReadyTrim(sd, false);
            soundIdle = false;
        }
        if (vent && armSwitch && !lastArm && !firing)
            sd.play(sfx::AFTERLIFE_BEEP_WAND_S5, false, config::WAND_BEEP_LEVEL_DB);
        lastArm = armSwitch;
    }

    // ProtonPack wandExtraSoundsStop().
    static void stopWandSounds(PackSdAudio &sd)
    {
        sd.stop(sfx::WAND_RAMP_1);
        sd.stop(sfx::WAND_IDLE_1);
        sd.stop(sfx::WAND_RAMP_2);
        sd.stop(sfx::WAND_IDLE_2);
        sd.stop(sfx::WAND_RAMP_DOWN_1);
        sd.stop(sfx::WAND_RAMP_DOWN_2);
        sd.stop(sfx::WAND_RAMP_2_FADE_IN);
        sd.stop(sfx::WAND_RAMP_DOWN_2_FADE_OUT);
    }
};
