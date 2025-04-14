#include "doom.hpp"
#include "box-emu.hpp"
#include "statistics.hpp"
#include "esp_log.h"

static const char* TAG = "DOOM";

static uint16_t* displayBuffer[2];
static uint8_t currentBuffer = 0;
static uint16_t* framebuffer = nullptr;
static uint16_t* audio_buffers[2];
static uint8_t currentAudioBuffer = 0;
static uint16_t* audio_buffer = nullptr;
static std::atomic<bool> frame_complete = false;

static bool initialized = false;
static bool unlock = false;


// prboom includes
extern "C" {
#include <prboom/doomtype.h>
#include <prboom/doomstat.h>
#include <prboom/doomdef.h>
#include <prboom/d_main.h>
#include <prboom/d_net.h>
#include <prboom/g_game.h>
#include <prboom/i_system.h>
#include <prboom/i_video.h>
#include <prboom/i_sound.h>
#include <prboom/i_main.h>
#include <prboom/m_argv.h>
#include <prboom/m_fixed.h>
#include <prboom/m_menu.h>
#include <prboom/m_misc.h>
#include <prboom/r_draw.h>
#include <prboom/r_fps.h>
#include <prboom/s_sound.h>
#include <prboom/st_stuff.h>

/////////////////////////////////////////////
// Copied from retro-go/prboom-go/main/main.c
/////////////////////////////////////////////

// 22050 reduces perf by almost 15% but 11025 sounds awful on the G32...
#define AUDIO_SAMPLE_RATE 11025

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

static channel_t channels[NUM_MIX_CHANNELS];
static const doom_sfx_t *sfx[NUMSFX];
static uint16_t mixbuffer[AUDIO_BUFFER_LENGTH];
// static const music_player_t *music_player = &opl_synth_player;
// static bool musicPlaying = false;

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

// static rg_gui_event_t gamma_update_cb(rg_gui_option_t *option, rg_gui_event_t event)
// {
//     int gamma = usegamma;
//     int max = 9;

//     if (event == RG_DIALOG_PREV)
//         gamma = gamma > 0 ? gamma - 1 : max;

//     if (event == RG_DIALOG_NEXT)
//         gamma = gamma < max ? gamma + 1 : 0;

//     if (gamma != usegamma)
//     {
//         usegamma = gamma;
//         rg_settings_set_number(NS_APP, SETTING_GAMMA, gamma);
//         I_SetPalette(current_palette);
//         return RG_DIALOG_REDRAW;
//     }

//     sprintf(option->value, "%d/%d", gamma, max);

//     return RG_DIALOG_VOID;
// }

void I_StartFrame(void)
{
    //
}

void I_UpdateNoBlit(void)
{
    //
}

void I_FinishUpdate(void)
{
    static auto& box = BoxEmu::get();
    box.push_frame(framebuffer);

    // swap buffers
    currentBuffer = currentBuffer ? 0 : 1;
    framebuffer = displayBuffer[currentBuffer];

    frame_complete = true;
}

bool I_StartDisplay(void)
{
    return true;
}

void I_EndDisplay(void)
{
    // TODO:
}

void I_SetPalette(int pal)
{
    uint16_t *palette = (uint16_t*)V_BuildPalette(pal, 16);
    // Z_Free(palette);
    static auto& box = BoxEmu::get();
    box.palette(palette);
}

void I_InitGraphics(void)
{
    // set first three to standard values
    for (int i = 0; i < 3; i++)
    {
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

int I_GetTimeMS(void)
{
    return esp_timer_get_time() / 1000;
}

int I_GetTime(void)
{
    return I_GetTimeMS() * TICRATE * realtic_clock_rate / 100000;
}

void I_uSleep(unsigned long usecs)
{
    // TODO: ?
    usleep(usecs);
}

void I_SafeExit(int rc)
{
    // TODO:
}

const char *I_DoomExeDir(void)
{
    return "";
}

void I_UpdateSoundParams(int handle, int volume, int seperation, int pitch)
{
}

int I_StartSound(int sfxid, int channel, int vol, int sep, int pitch, int priority)
{
    int oldest = gametic;
    int slot = 0;

    // Unknown sound
    if (!sfx[sfxid])
        return -1;

    // These sound are played only once at a time. Stop any running ones.
    if (sfxid == sfx_sawup || sfxid == sfx_sawidl || sfxid == sfx_sawful
        || sfxid == sfx_sawhit || sfxid == sfx_stnmov || sfxid == sfx_pistol)
    {
        for (int i = 0; i < NUM_MIX_CHANNELS; i++)
        {
            if (channels[i].sfx == sfx[sfxid])
                channels[i].sfx = NULL;
        }
    }

    // Find available channel or steal the oldest
    for (int i = 0; i < NUM_MIX_CHANNELS; i++)
    {
        if (channels[i].sfx == NULL)
        {
            slot = i;
            break;
        }
        else if (channels[i].starttic < oldest)
        {
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

void I_StopSound(int handle)
{
    if (handle < NUM_MIX_CHANNELS)
        channels[handle].sfx = NULL;
}

bool I_SoundIsPlaying(int handle)
{
    // return (handle < NUM_MIX_CHANNELS && channels[handle].sfx);
    return false;
}

bool I_AnySoundStillPlaying(void)
{
    for (int i = 0; i < NUM_MIX_CHANNELS; i++)
        if (channels[i].sfx)
            return true;
    return false;
}

static void soundTask(void *arg)
{
    while (1)
    {
        int16_t *audioBuffer = (int16_t *)mixbuffer;
        int16_t *audioBufferEnd = audioBuffer + AUDIO_BUFFER_LENGTH * 2;
        int16_t stream[2];

        while (audioBuffer < audioBufferEnd)
        {
            int totalSample = 0;
            int totalSources = 0;
            int sample;

            if (snd_SfxVolume > 0)
            {
                for (int i = 0; i < NUM_MIX_CHANNELS; i++)
                {
                    channel_t *chan = &channels[i];
                    if (!chan->sfx)
                        continue;

                    size_t pos = (size_t)(chan->pos++ * chan->factor);

                    if (pos >= chan->sfx->length)
                    {
                        chan->sfx = NULL;
                    }
                    else if ((sample = chan->sfx->samples[pos]))
                    {
                        totalSample += sample - 127;
                        totalSources++;
                    }
                }

                totalSample <<= 7;
                totalSample /= (16 - snd_SfxVolume);
            }

            // if (musicPlaying && snd_MusicVolume > 0)
            // {
            //     music_player->render(&stream, 1); // It returns 2 (stereo) 16bits values per sample
            //     sample = stream[0]; // [0] and [1] are the same value
            //     if (sample > 0)
            //     {
            //         totalSample += sample / (16 - snd_MusicVolume);
            //         if (totalSources == 0)
            //             totalSources = 1;
            //     }
            // }

            if (totalSources > 0)
                totalSample /= totalSources;

            if (totalSample > 32767)
                totalSample = 32767;
            else if (totalSample < -32768)
                totalSample = -32768;

            *audioBuffer++ = totalSample;
            *audioBuffer++ = totalSample;
        }

        // TODO:
        // rg_audio_submit(mixbuffer, AUDIO_BUFFER_LENGTH);

    }
}

void I_InitSound(void)
{
    // TODO:
    // for (int i = 1; i < NUMSFX; i++)
    // {
    //     if (S_sfx[i].lumpnum != -1)
    //         sfx[i] = (const doom_sfx_t*)W_CacheLumpNum(S_sfx[i].lumpnum);
    // }

    // TODO:
    // music_player->init(snd_samplerate);
    // music_player->setvolume(snd_MusicVolume);

    // TODO:
    // rg_task_create("doom_sound", &soundTask, NULL, 2048, RG_TASK_PRIORITY_2, 1);
}

void I_ShutdownSound(void)
{
    // music_player->shutdown();
}

void I_PlaySong(int handle, int looping)
{
    // music_player->play((void *)handle, looping);
    // musicPlaying = true;
}

void I_PauseSong(int handle)
{
    // music_player->pause();
    // musicPlaying = false;
}

void I_ResumeSong(int handle)
{
    // music_player->resume();
    // musicPlaying = true;
}

void I_StopSong(int handle)
{
    // music_player->stop();
    // musicPlaying = false;
}

void I_UnRegisterSong(int handle)
{
    // music_player->unregistersong((void *)handle);
}

int I_RegisterSong(const void *data, size_t len)
{
    uint8_t *mid = NULL;
    size_t midlen;
    int handle = 0;

    // if (mus2mid(data, len, &mid, &midlen, 64) == 0)
    //     handle = (int)music_player->registersong(mid, midlen);
    // else
    //     handle = (int)music_player->registersong(data, len);

    // free(mid);

    return handle;
}

void I_SetMusicVolume(int volume)
{
    static auto& box = BoxEmu::get();
    box.volume(volume / 100.0f);
}

void I_StartTic(void)
{
    // static int64_t last_time = 0;
    // static int32_t prev_joystick = 0x0000;
    // static int32_t rg_menu_delay = 0;
    // uint32_t joystick = rg_input_read_gamepad();
    // uint32_t changed = prev_joystick ^ joystick;
    // event_t event = {0};

    // // Long press on menu will open retro-go's menu if needed, instead of DOOM's.
    // // This is still needed to quit (DOOM 2) and for the debug menu. We'll unify that mess soon...
    // if (joystick & (RG_KEY_MENU|RG_KEY_OPTION))
    // {
    //     if (joystick & RG_KEY_OPTION)
    //     {
    //         Z_FreeTags(PU_CACHE, PU_CACHE); // At this point the heap is usually full. Let's reclaim some!
    //         rg_gui_options_menu();
    //         changed = 0;
    //     }
    //     else if (rg_menu_delay++ == TICRATE / 2)
    //     {
    //         Z_FreeTags(PU_CACHE, PU_CACHE); // At this point the heap is usually full. Let's reclaim some!
    //         rg_gui_game_menu();
    //     }
    //     realtic_clock_rate = app->speed * 100;
    //     R_InitInterpolation();
    // }
    // else
    // {
    //     rg_menu_delay = 0;
    // }

    // if (changed)
    // {
    //     for (int i = 0; i < RG_COUNT(keymap); i++)
    //     {
    //         if (changed & keymap[i].mask)
    //         {
    //             event.type = (joystick & keymap[i].mask) ? ev_keydown : ev_keyup;
    //             event.data1 = *keymap[i].key;
    //             D_PostEvent(&event);
    //         }
    //     }
    // }

    // rg_system_tick(rg_system_timer() - last_time);
    // last_time = rg_system_timer();
    // prev_joystick = joystick;
}

void I_Init(void)
{
    snd_channels = NUM_MIX_CHANNELS;
    snd_samplerate = AUDIO_SAMPLE_RATE;
    snd_MusicVolume = 15;
    snd_SfxVolume = 15;
    // usegamma = rg_settings_get_number(NS_APP, SETTING_GAMMA, 0);
}
} // extern "C"

void reset_doom() {
    // Reset game state
    // D_DoomLoop();
}

void init_doom(const std::string& wad_filename, uint8_t *wad_data, size_t wad_data_size) {
    // Initialize system interface
    if (!I_StartDisplay()) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }

    // Initialize graphics
    I_InitGraphics();

    // Set native size and palette
    BoxEmu::get().native_size(320, 200);
    BoxEmu::get().palette(nullptr);

    // Set audio sample rate
    BoxEmu::get().audio_sample_rate(44100);

    // Initialize sound
    I_InitSound();

    fmt::print("Loading WAD: {}\n", wad_filename);

    // myargv = (const char *[]){"doom", "-save", "/sdcard/doom", "-iwad", wad_filename.c_str()};
    myargv = new const char*[5];
    myargv[0] = "doom";
    myargv[1] = "-save";
    myargv[2] = "/sdcard/doom";
    myargv[3] = "-iwad";
    myargv[4] = wad_filename.c_str();
    myargc = 5;

    // myargv = (const char *[]){"doom", "-save", "/sdcard/doom", "-iwad", wad_filename.c_str(), "-file", "/sdcard/doom/prboom.wad"};
    // myargc = 7;

    // Initialize game
    Z_Init();
    D_DoomMainSetup();

    fmt::print("Loaded WAD: {}\n", wad_filename);

    reset_frame_time();
}

void run_doom_rom() {
    auto start = esp_timer_get_time();

    // // Handle input
    // auto state = BoxEmu::get().gamepad_state();

    // // // Map gamepad to Doom controls
    // // if (state.up) I_HandleKey(KEY_UPARROW);
    // // if (state.down) I_HandleKey(KEY_DOWNARROW);
    // // if (state.left) I_HandleKey(KEY_LEFTARROW);
    // // if (state.right) I_HandleKey(KEY_RIGHTARROW);
    // // if (state.a) I_HandleKey(KEY_FIRE);
    // // if (state.b) I_HandleKey(KEY_USE);

    // // Update game state
    // // From:
    // // D_DoomLoop();

      WasRenderedInTryRunTics = false;
      // frame syncronous IO operations
      I_StartFrame ();

      if (ffmap == gamemap) ffmap = 0;

      // process one or more tics
      if (singletics)
        {
          I_StartTic ();
          G_BuildTiccmd (&netcmds[consoleplayer][maketic%BACKUPTICS]);
          if (advancedemo)
            D_DoAdvanceDemo ();
          M_Ticker ();
          G_Ticker ();
          gametic++;
          maketic++;
        }
      else
        TryRunTics (); // will run at least one tic

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
    static constexpr uint64_t max_frame_time = 1000000 / 60;
    if (!unlock && elapsed < max_frame_time) {
        auto sleep_time = (max_frame_time - elapsed) / 1e3;
        std::this_thread::sleep_for(sleep_time * std::chrono::milliseconds(1));
    }
}

void load_doom(std::string_view save_path) {
    if (save_path.size()) {
        // TODO:
        // G_LoadGame(save_path.data());
    }
}

void save_doom(std::string_view save_path) {
    if (save_path.size()) {
        // TODO:
        // G_SaveGame(save_path.data());
    }
}

std::vector<uint8_t> get_doom_video_buffer() {
    std::vector<uint8_t> frame(320 * 200 * 2);
    memcpy(frame.data(), framebuffer, frame.size());
    return frame;
}

void deinit_doom() {
    // Shutdown graphics
    // I_ShutdownGraphics();
        
    // End display
    I_EndDisplay();
        
    Z_FreeTags(0, PU_MAX);

    BoxEmu::get().audio_sample_rate(48000);
}
