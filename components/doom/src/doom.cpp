#include "doom.hpp"

#include "box-emu.hpp"
#include "pool_allocator.h"
#include "shared_memory.h"
#include "statistics.hpp"

using namespace std::chrono_literals;

static uint16_t* displayBuffer[2];
static uint8_t currentBuffer = 0;
static uint16_t* framebuffer = nullptr;

static bool unlock = false;

static uint16_t doom_palette[256];

// Range is [0, 15]
static constexpr int DEFAULT_AUDIO_VOLUME = 15;
static std::unique_ptr<espp::Task> audio_task;

static const char *doom_argv[10];

enum class WeaponHaptics : int {
    FIST = 3,
    PISTOL = 2,
    SHOTGUN = 10,
    CHAINGUN = 12,
    ROCKET_LAUNCHER = 27,
    PLASMA_RIFLE = 14,
    BFG9000 = 47,
    CHAINSAW = 15,
    SUPER_SHOTGUN = 52
};

// NOTE: The order of the enum values must match the order of the weapons in the
//       game, which is the wp_* enum values defined in doomdef.h
static constexpr int WeaponHapticLookup[] = {
    (int)WeaponHaptics::FIST,
    (int)WeaponHaptics::PISTOL,
    (int)WeaponHaptics::SHOTGUN,
    (int)WeaponHaptics::CHAINGUN,
    (int)WeaponHaptics::ROCKET_LAUNCHER,
    (int)WeaponHaptics::PLASMA_RIFLE,
    (int)WeaponHaptics::BFG9000,
    (int)WeaponHaptics::CHAINSAW,
    (int)WeaponHaptics::SUPER_SHOTGUN
};

// prboom includes
extern "C" {
    /////////////////////////////////////////////
    // Copied from retro-go/prboom-go/main/main.c
    /////////////////////////////////////////////

#include <doomtype.h>
#include <doomstat.h>
#include <doomdef.h>
#include <d_main.h>
#include <d_net.h>
#include <g_game.h>
#include <i_system.h>
#include <i_video.h>
#include <i_sound.h>
#include <i_main.h>
#include <m_argv.h>
#include <m_fixed.h>
#include <m_menu.h>
#include <m_misc.h>
#include <r_draw.h>
#include <r_fps.h>
#include <s_sound.h>
#include <st_stuff.h>
#include <mus2mid.h>
#include <midifile.h>
#include <oplplayer.h>

#define AUDIO_SAMPLE_RATE (22050 / 2)

#define AUDIO_BUFFER_LENGTH (AUDIO_SAMPLE_RATE / TICRATE + 1)
#define NUM_MIX_CHANNELS 8

    // Expected variables by doom
    int snd_card = 1, mus_card = 1;
    int snd_samplerate = AUDIO_SAMPLE_RATE;
    int current_palette = 0;

    typedef struct {
        uint16_t unused1;
        uint16_t samplerate;
        uint16_t length;
        uint16_t unused2;
        byte samples[];
    } doom_sfx_t;

    typedef struct {
        const doom_sfx_t *sfx;
        size_t pos;
        float factor;
        int starttic;
    } channel_t;

    static channel_t *channels=nullptr; // [NUM_MIX_CHANNELS];
    static const doom_sfx_t **sfx = nullptr; // [NUMSFX];
    static uint16_t *mixbuffer = nullptr; // [AUDIO_BUFFER_LENGTH];
    static const music_player_t *music_player = &opl_synth_player;
    static bool musicPlaying = false;

    static const struct {int mask; int *key;} keymap[] = {
        {(int)GamepadState::Button::UP, &key_up},
        {(int)GamepadState::Button::DOWN, &key_down},
        {(int)GamepadState::Button::LEFT, &key_left},
        {(int)GamepadState::Button::RIGHT, &key_right},
        {(int)GamepadState::Button::A, &key_fire},
        {(int)GamepadState::Button::A, &key_enter},
        {(int)GamepadState::Button::B, &key_speed},
        {(int)GamepadState::Button::B, &key_strafe},
        {(int)GamepadState::Button::B, &key_backspace},
        {(int)GamepadState::Button::START, &key_escape},
        {(int)GamepadState::Button::SELECT, &key_map},
        {(int)GamepadState::Button::X, &key_use},
        {(int)GamepadState::Button::Y, &key_weapontoggle},
    };

    void R_PlayerFire(player_t *player) {
        static auto& box = BoxEmu::get();
        int weapon_fired = player->readyweapon;
        int haptic_effect_index = WeaponHapticLookup[weapon_fired];
        box.play_haptic_effect(haptic_effect_index);
    }

    void R_PlayerHurt(player_t *player, int damage, int saved) {
        static auto& box = BoxEmu::get();
        int haptic_effect_index = 0;
        if (damage > 5) {
            // 70 - transition ramp down long smooth 1 - 100 to 0%
            // 75 - transition ramp down short smooth 2 - 100 to 0%
            haptic_effect_index = saved > 0 ? 70 : 75;
        } else if (damage > 0) {
            // 78 - transition ramp down medium sharp 1 - 100 to 0%
            // 64 - transition hum 100%
            haptic_effect_index = saved > 0 ? 78 : 64;
        }
        box.play_haptic_effect(haptic_effect_index);
    }

    void R_PlayerPickupWeapon(player_t *player, int weapon) {
        static auto& box = BoxEmu::get();
        // play 29 (short double click strong 3 - 60%)
        box.play_haptic_effect(29);
    }

    void R_PlayerPickupAmmo(player_t *player, ammotype_t ammo, int num) {
        static auto& box = BoxEmu::get();
        // play 34 (short double sharp tick 1 - 100%)
        box.play_haptic_effect(34);
    }

    void R_PlayerPickupHealth(player_t *player, int health) {
        static auto& box = BoxEmu::get();
        // play 18 (strong click 2 - 80%)
        box.play_haptic_effect(18);
    }

    void R_PlayerPickupArmor(player_t *player, int armor) {
        static auto& box = BoxEmu::get();
        // play 19 (strong click 3 - 60%)
        box.play_haptic_effect(19);
    }

    void R_PlayerPickupCard(player_t *player, card_t card) {
        static auto& box = BoxEmu::get();
        // play 5 (sharp click - 60%)
        box.play_haptic_effect(5);
    }

    void R_PlayerPickupPowerUp(player_t *player, int powerup) {
        static auto& box = BoxEmu::get();
        // play 12 (triple click - 100%)
        box.play_haptic_effect(12);
    }

    void I_StartFrame(void) {
    }

    void I_UpdateNoBlit(void) {
    }

    void I_FinishUpdate(void) {
        static auto& box = BoxEmu::get();
        box.push_frame(framebuffer);
    }

    bool I_StartDisplay(void) {
        return true;
    }

    void I_EndDisplay(void) {
    }

    void I_SetPalette(int pal) {
        static auto& box = BoxEmu::get();
        uint16_t *palette = (uint16_t*)V_BuildPalette(pal, 16);
        if (palette == NULL) {
            box.palette(NULL);
            return;
        }
        // copy palette to doom_palette
        for (int i = 0; i < 256; i++) {
            doom_palette[i] = palette[i];
        }
        // release the allocated memory
        Z_Free(palette);
        // set the palette
        box.palette(doom_palette);
    }

    void I_InitGraphics(void) {
        // Set native size and palette
        BoxEmu::get().native_size(SCREENWIDTH, SCREENHEIGHT);

        displayBuffer[0] = (uint16_t*)BoxEmu::get().frame_buffer0();
        displayBuffer[1] = (uint16_t*)BoxEmu::get().frame_buffer1();
        currentBuffer = 0;
        framebuffer = displayBuffer[currentBuffer];

        // set first three to standard values
        for (int i = 0; i < 3; i++) {
            screens[i].width = SCREENWIDTH;
            screens[i].height = SCREENHEIGHT;
            screens[i].byte_pitch = SCREENWIDTH;
        }

        // Main screen uses internal ram for speed
        screens[0].data = (uint8_t*)framebuffer;
        screens[0].not_on_heap = true;

        // statusbar
        screens[4].width = SCREENWIDTH;
        screens[4].height = (ST_SCALED_HEIGHT + 1);
        screens[4].byte_pitch = SCREENWIDTH;
    }

    int I_GetTimeMS(void) {
        return esp_timer_get_time() / 1000;
    }

    int I_GetTime(void) {
        return I_GetTimeMS() * TICRATE * realtic_clock_rate / 100000;
    }

    void I_uSleep(unsigned long usecs) {
        // TODO: ?
        usleep(usecs);
    }

    void I_SafeExit(int rc) {
        // TODO:
    }

    const char *I_DoomExeDir(void) {
        return "";
    }

    void I_UpdateSoundParams(int handle, int volume, int seperation, int pitch)
    {
    }

    int I_StartSound(int sfxid, int channel, int vol, int sep, int pitch, int priority) {
        int oldest = gametic;
        int slot = 0;

        // Unknown sound
        if (!sfx[sfxid])
            return -1;

        // These sound are played only once at a time. Stop any running ones.
        if (sfxid == sfx_sawup || sfxid == sfx_sawidl || sfxid == sfx_sawful
            || sfxid == sfx_sawhit || sfxid == sfx_stnmov || sfxid == sfx_pistol) {
            for (int i = 0; i < NUM_MIX_CHANNELS; i++) {
                if (channels[i].sfx == sfx[sfxid])
                    channels[i].sfx = NULL;
            }
        }

        // Find available channel or steal the oldest
        for (int i = 0; i < NUM_MIX_CHANNELS; i++) {
            if (channels[i].sfx == NULL) {
                slot = i;
                break;
            } else if (channels[i].starttic < oldest) {
                slot = i;
                oldest = channels[i].starttic;
            }
        }

        channel_t *chan = &channels[slot];
        chan->sfx = sfx[sfxid];
        chan->factor = (float)chan->sfx->samplerate / AUDIO_SAMPLE_RATE;
        chan->pos = 0;

        return slot;
    }

    void I_StopSound(int handle) {
        if (handle < NUM_MIX_CHANNELS)
            channels[handle].sfx = NULL;
    }

    bool I_SoundIsPlaying(int handle) {
        return false;
    }

    bool I_AnySoundStillPlaying(void) {
        for (int i = 0; i < NUM_MIX_CHANNELS; i++)
            if (channels[i].sfx)
                return true;
        return false;
    }

    static bool soundTask() {
        bool haveMusic = snd_MusicVolume > 0 && musicPlaying;
        bool haveSFX = snd_SfxVolume > 0 && I_AnySoundStillPlaying();

        if (haveMusic) {
            music_player->render(mixbuffer, AUDIO_BUFFER_LENGTH);
        }

        if (haveSFX) {
            int16_t *audioBuffer = (int16_t *)mixbuffer;
            const int16_t *audioBufferEnd = audioBuffer + AUDIO_BUFFER_LENGTH;
            while (audioBuffer < audioBufferEnd) {
                int totalSample = 0;
                int totalSources = 0;
                int sample;

                for (int i = 0; i < NUM_MIX_CHANNELS; i++) {
                    channel_t *chan = &channels[i];
                    if (!chan->sfx)
                        continue;

                    size_t pos = (size_t)(chan->pos++ * chan->factor);

                    if (pos >= chan->sfx->length) {
                        chan->sfx = NULL;
                    } else if ((sample = chan->sfx->samples[pos])) {
                        totalSample += sample - 127;
                        totalSources++;
                    }
                }

                totalSample <<= 7;
                totalSample /= (16 - snd_SfxVolume);

                if (haveMusic) {
                    totalSample += *audioBuffer;
                    totalSources += (totalSources == 0);
                }

                if (totalSources > 0)
                    totalSample /= totalSources;

                if (totalSample > 32767)
                    totalSample = 32767;
                else if (totalSample < -32768)
                    totalSample = -32768;

                *audioBuffer++ = totalSample;
                *audioBuffer++ = totalSample;
            }
        }

        if (!haveMusic && !haveSFX) {
            memset(mixbuffer, 0, AUDIO_BUFFER_LENGTH * sizeof(int16_t));
        }

        static auto& box = BoxEmu::get();
        box.play_audio((const uint8_t*)mixbuffer, AUDIO_BUFFER_LENGTH * sizeof(int16_t));
        std::this_thread::sleep_for(std::chrono::microseconds(1'000'000 / TICRATE));
        return false;
    }

    void I_InitSound(void) {
        for (int i = 1; i < NUMSFX; i++) {
            if (S_sfx[i].lumpnum != -1) {
                sfx[i] = (const doom_sfx_t*)W_CacheLumpNum(S_sfx[i].lumpnum);
            }
        }

        music_player->init(snd_samplerate);
        music_player->setvolume(snd_MusicVolume);

        auto& box = BoxEmu::get();
        box.audio_sample_rate(snd_samplerate);

        // ensure any previous task is stopped and freed
        audio_task.reset();
        // make the audio task
        audio_task = espp::Task::make_unique(espp::Task::Config{
                .callback = soundTask,
                .task_config = {
                    .name = "doom_sound",
                    .stack_size_bytes = 2048,
                    .priority = 10,
                    .core_id = 1,
                }
            });
        // now start the task
        audio_task->start();
    }

    void I_ShutdownSound(void) {
        music_player->shutdown();
        audio_task.reset();
    }

    void I_PlaySong(int handle, int looping) {
        music_player->play((void *)handle, looping);
        musicPlaying = true;
    }

    void I_PauseSong(int handle) {
        music_player->pause();
        musicPlaying = false;
    }

    void I_ResumeSong(int handle) {
        music_player->resume();
        musicPlaying = true;
    }

    void I_StopSong(int handle) {
        music_player->stop();
        musicPlaying = false;
    }

    void I_UnRegisterSong(int handle) {
        music_player->unregistersong((void *)handle);
    }

    int I_RegisterSong(const void *data, size_t len) {
        uint8_t *mid = NULL;
        size_t midlen;
        int handle = 0;

        if (mus2mid((const byte*)data, len, &mid, &midlen, 64) == 0)
            handle = (int)music_player->registersong(mid, midlen);
        else
            handle = (int)music_player->registersong(data, len);

        free(mid);

        return handle;
    }

    void I_SetMusicVolume(int volume) {
        music_player->setvolume(volume);
    }

    void I_StartTic(void) {
        static int32_t prev_joystick = 0x0000;

        static auto& box = BoxEmu::get();
        auto state = box.gamepad_state();
        auto joystick = state.buttons;

        // update unlock based on x button
        static bool last_x = false;
        if (state.x && !last_x) {
            unlock = !unlock;
        }
        last_x = state.x;

        uint32_t changed = prev_joystick ^ joystick;
        event_t event = {};

        if (changed) {
            for (int i = 0; i < std::size(keymap); i++) {
                auto key_bit = 1 << keymap[i].mask;
                if (changed & key_bit) {
                    event.type = (joystick & key_bit) ? ev_keydown : ev_keyup;
                    event.data1 = *keymap[i].key;
                    D_PostEvent(&event);
                }
            }
        }

        prev_joystick = joystick;
    }

    void I_Init(void) {
        snd_channels = NUM_MIX_CHANNELS;
        snd_samplerate = AUDIO_SAMPLE_RATE;
        snd_MusicVolume = DEFAULT_AUDIO_VOLUME;
        snd_SfxVolume = DEFAULT_AUDIO_VOLUME;
        usegamma = 0;
    }
} // extern "C"

static std::string doom_wad_path = "";
void reset_doom() {
    deinit_doom();
    init_doom(doom_wad_path, nullptr, 0);
}

void init_doom(const std::string& wad_filename, uint8_t *wad_data, size_t wad_data_size) {
    doom_wad_path = wad_filename;

    // reset unlock
    unlock = false;

    // needed for doom.cpp
    channels = (channel_t *)shared_malloc(sizeof(channel_t) * NUM_MIX_CHANNELS);
    sfx = (const doom_sfx_t **)shared_malloc(sizeof(doom_sfx_t*) * NUMSFX);
    mixbuffer = (uint16_t *)shared_malloc(sizeof(uint16_t) * AUDIO_BUFFER_LENGTH);

    doom_init_shared_memory();

    SCREENWIDTH = MAX_SCREENWIDTH;
    SCREENHEIGHT = MAX_SCREENHEIGHT;

    fmt::print("Loading WAD: {}\n", wad_filename);

    myargv = doom_argv;
    myargc = 5;
    doom_argv[0] = "doom";
    doom_argv[1] = "-save";
    doom_argv[2] = "/sdcard/saves";
    doom_argv[3] = "-iwad";
    doom_argv[4] = wad_filename.c_str();
    // doom_argv[5] = "-file";
    // doom_argv[6] = pwad;
    doom_argv[myargc] = 0;

    // Initialize game
    void* pool_ptr = (void*)BoxEmu::get().romdata();
    pool_create(pool_ptr, 4 * 1024 * 1024);
    // Z_SetPoolPointer(pool_ptr, 4 * 1024 * 1024);
    Z_Init();
    D_DoomMainSetup();

    fmt::print("Loaded WAD: {}\n", wad_filename);

    reset_frame_time();
}

void run_doom_rom() {
    auto start = esp_timer_get_time();

    // Update game state
    // Copied From: D_DoomLoop();

    WasRenderedInTryRunTics = false;
    // frame syncronous IO operations
    I_StartFrame ();

    if (ffmap == gamemap) ffmap = 0;

    // process one or more tics
    if (singletics) {
        I_StartTic ();
        G_BuildTiccmd (&netcmds[consoleplayer][maketic%BACKUPTICS]);
        if (advancedemo)
            D_DoAdvanceDemo ();
        M_Ticker ();
        G_Ticker ();
        gametic++;
        maketic++;
    } else {
        TryRunTics (); // will run at least one tic
    }

    // killough 3/16/98: change consoleplayer to displayplayer
    if (players[displayplayer].mo) // cph 2002/08/10
        S_UpdateSounds(players[displayplayer].mo);// move positional sounds

    // Update display, next frame, with current state.
    if (!movement_smooth || !WasRenderedInTryRunTics || gamestate != wipegamestate)
        D_Display();

    // Handle timing
    auto end = esp_timer_get_time();
    uint64_t elapsed = end - start;
    update_frame_time(elapsed);

    static constexpr uint64_t max_frame_time = 1'000'000 / TICRATE;
    if (!unlock && elapsed < max_frame_time) {
        auto sleep_time = (max_frame_time - elapsed);
        std::this_thread::sleep_for(sleep_time * 1us);
    }
}

void pause_doom_tasks() {
    snd_MusicVolume = 0;
    snd_SfxVolume = 0;
}

void resume_doom_tasks() {
    snd_MusicVolume = DEFAULT_AUDIO_VOLUME;
    snd_SfxVolume = DEFAULT_AUDIO_VOLUME;
}

void load_doom(std::string_view save_path, int save_slot) {
    if (save_slot >= 0) {
        // set the save game file name
        G_SetSaveGameFileName(const_cast<char*>(save_path.data()), save_path.size());
        // load the game
        bool command = false;
        G_LoadGame(save_slot, command);
        G_DoLoadGame();
    }
}

void save_doom(std::string_view save_path, int save_slot) {
    if (save_slot >= 0) {
        // set the save game file name
        G_SetSaveGameFileName(const_cast<char*>(save_path.data()), save_path.size());
        // save the game
        auto description = fmt::format("Save Slot {}", save_slot);
        G_SaveGame(save_slot, const_cast<char*>(description.c_str()));
        G_DoSaveGame(true);
    }
}

std::span<uint8_t> get_doom_video_buffer() {
    size_t num_pixels = SCREENWIDTH * SCREENHEIGHT;
    uint8_t *span_ptr = (uint8_t*)(currentBuffer ? displayBuffer[0] : displayBuffer[1]);

    std::span<uint8_t> frame(span_ptr, num_pixels * sizeof(uint16_t));
    // use the palette to convert the framebuffer to RGB565
    const uint8_t *buf = (const uint8_t*)framebuffer;
    const uint16_t *palette = BoxEmu::get().palette();
    if (palette) {
        for (int i = 0; i < num_pixels; i++) {
            uint8_t pal_index = buf[i];
            uint16_t color = palette[pal_index];
            frame[i * 2] = color & 0xFF;
            frame[i * 2 + 1] = (color >> 8) & 0xFF;
        }
    }
    return frame;
}

void deinit_doom() {
    // stop the audio task
    audio_task.reset();
    // End display
    I_EndDisplay();
    // Free memory
    Z_Close();
    // reset audio state
    BoxEmu::get().audio_sample_rate(48000);
    shared_mem_clear();
    pool_destroy();
}
