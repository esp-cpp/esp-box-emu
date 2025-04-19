#include "doom.hpp"
#include "box-emu.hpp"
#include "statistics.hpp"
#include "esp_log.h"
#include "shared_memory.h"

static const char* TAG = "DOOM";

static uint16_t* displayBuffer[2];
static uint8_t currentBuffer = 0;
static uint16_t* framebuffer = nullptr;
static uint16_t* audio_buffers[2];
static uint8_t currentAudioBuffer = 0;
static uint16_t* audio_buffer = nullptr;

static uint16_t doom_palette[256];

static std::unique_ptr<espp::Task> audio_task;

static bool unlock = false;

static const char *doom_argv[10];

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
#include <prboom/mus2mid.h>
#include <prboom/midifile.h>
#include <prboom/oplplayer.h>

#include <prboom/dbopl.h>
#include <prboom/opl.h>
#include <prboom/r_plane.h>

#include <prboom/hu_lib.h>
#include <prboom/hu_stuff.h>

/////////////////////////////////////////////
// Copied from retro-go/prboom-go/main/main.c
/////////////////////////////////////////////

#define AUDIO_SAMPLE_RATE 22050

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
}

void I_UpdateNoBlit(void)
{
}

void I_FinishUpdate(void)
{
    static auto& box = BoxEmu::get();
    box.push_frame(framebuffer);
    // // swap buffers
    // currentBuffer = currentBuffer ? 0 : 1;
    // framebuffer = displayBuffer[currentBuffer];
    // screens[0].data = (uint8_t*)framebuffer;
}

bool I_StartDisplay(void)
{
    return true;
}

void I_EndDisplay(void)
{
}

void I_SetPalette(int pal)
{
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

void I_InitGraphics(void)
{
    fmt::print("Init graphics\n");

    // Set native size and palette
    BoxEmu::get().native_size(SCREENWIDTH, SCREENHEIGHT);

    displayBuffer[0] = (uint16_t*)BoxEmu::get().frame_buffer0();
    displayBuffer[1] = (uint16_t*)BoxEmu::get().frame_buffer1();
    currentBuffer = 0;
    framebuffer = displayBuffer[currentBuffer];

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
    return false;
}

bool I_AnySoundStillPlaying(void)
{
    for (int i = 0; i < NUM_MIX_CHANNELS; i++)
        if (channels[i].sfx)
            return true;
    return false;
}

static bool soundTask() {
    bool haveMusic = snd_MusicVolume > 0 && musicPlaying;
    bool haveSFX = snd_SfxVolume > 0 && I_AnySoundStillPlaying();

    if (haveMusic)
    {
        music_player->render(mixbuffer, AUDIO_BUFFER_LENGTH);
    }

    if (haveSFX)
        {
            int16_t *audioBuffer = (int16_t *)mixbuffer;
            int16_t *audioBufferEnd = audioBuffer + AUDIO_BUFFER_LENGTH;
            while (audioBuffer < audioBufferEnd)
                {
                    int totalSample = 0;
                    int totalSources = 0;
                    int sample;

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

                    if (haveMusic)
                        {
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
                    // *audioBuffer++ = totalSample;
                }
        }

    if (!haveMusic && !haveSFX)
        {
            memset(mixbuffer, 0, sizeof(mixbuffer));
        }

    // rg_audio_submit(mixbuffer, AUDIO_BUFFER_LENGTH);
    static auto& box = BoxEmu::get();
    box.play_audio((const uint8_t*)mixbuffer, AUDIO_BUFFER_LENGTH * sizeof(int16_t));
    // std::this_thread::sleep_for(std::chrono::microseconds(1000 / 60));
    return false;
}

void I_InitSound(void)
{
    for (int i = 1; i < NUMSFX; i++)
    {
        if (S_sfx[i].lumpnum != -1) {
            sfx[i] = (const doom_sfx_t*)W_CacheLumpNum(S_sfx[i].lumpnum);
        }
    }

    music_player->init(snd_samplerate);
    music_player->setvolume(snd_MusicVolume);

    auto& box = BoxEmu::get();
    box.audio_sample_rate(snd_samplerate);

    audio_task.reset();
    audio_task = espp::Task::make_unique(espp::Task::Config{
            .callback = soundTask,
            .task_config = {
                .name = "doom_sound",
                .stack_size_bytes = 2048,
                .priority = 10,
                .core_id = 1,
            }
        });
}

void I_ShutdownSound(void)
{
    music_player->shutdown();
    audio_task.reset();
}

void I_PlaySong(int handle, int looping)
{
    music_player->play((void *)handle, looping);
    musicPlaying = true;
}

void I_PauseSong(int handle)
{
    music_player->pause();
    musicPlaying = false;
}

void I_ResumeSong(int handle)
{
    music_player->resume();
    musicPlaying = true;
}

void I_StopSong(int handle)
{
    music_player->stop();
    musicPlaying = false;
}

void I_UnRegisterSong(int handle)
{
    music_player->unregistersong((void *)handle);
}

int I_RegisterSong(const void *data, size_t len)
{
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

void I_SetMusicVolume(int volume)
{
    static auto& box = BoxEmu::get();
    box.volume(volume / 100.0f);
}

void I_StartTic(void)
{
    static int64_t last_time = 0;
    static int32_t prev_joystick = 0x0000;

    static auto& box = BoxEmu::get();
    auto state = box.gamepad_state();
    auto joystick = state.buttons;

    uint32_t changed = prev_joystick ^ joystick;
    event_t event = {};

    if (changed)
    {
        for (int i = 0; i < std::size(keymap); i++)
        {
            auto key_bit = 1 << keymap[i].mask;
            if (changed & key_bit)
            {
                event.type = (joystick & key_bit) ? ev_keydown : ev_keyup;
                event.data1 = *keymap[i].key;
                D_PostEvent(&event);
            }
        }
    }

    last_time = esp_timer_get_time();
    prev_joystick = joystick;
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

static std::string doom_wad_path = "";
void reset_doom() {
    deinit_doom();
    init_doom(doom_wad_path, nullptr, 0);
}

// From dbopl
#if ( DBOPL_WAVE == WAVE_HANDLER ) || ( DBOPL_WAVE == WAVE_TABLELOG )
extern Bit16u *ExpTable;
#endif
#if ( DBOPL_WAVE == WAVE_HANDLER )
//PI table used by WAVEHANDLER
extern Bit16u *SinTable;
#endif
extern Bit16s *WaveTable; // [ 8 * 512 ];
#if ( DBOPL_WAVE == WAVE_TABLEMUL )
extern Bit16u *MulTable; // [ 384 ];
#endif

extern Bit8u *KslTable; // [ 8 * 16 ];
extern Bit8u *TremoloTable; // [ TREMOLO_TABLE ];
extern Bit16u *ChanOffsetTable; // [32];
extern Bit16u *OpOffsetTable; // [64];

// from doomstat.h / g_game.c
extern player_t *players; // [MAXPLAYERS];
extern byte *gamekeydown; // [NUMKEYS];

// from info.c
extern mobjinfo_t *mobjinfo; // [NUMMOBJTYPES];
extern const mobjinfo_t init_mobjinfo[NUMMOBJTYPES];

// from m_cheat.c/h
extern struct cheat_s *cheat;
extern const struct cheat_s init_cheat[];
extern size_t init_cheat_bytes;

// from p_mobj.c
extern mapthing_t *itemrespawnque;
extern int *itemrespawntime;

// from sounds.c/h
extern musicinfo_t *S_music;
extern const size_t s_music_bytes;
extern const musicinfo_t init_S_music[];
extern sfxinfo_t *S_sfx;
extern const sfxinfo_t init_S_sfx[];
extern const size_t s_sfx_bytes;

// from st_stuff.c/h
extern patchnum_t *shortnum; // [10];
extern patchnum_t *keys; // [NUMCARDS+3];
extern patchnum_t *faces; // [ST_NUMFACES];
extern patchnum_t *arms; // [6][2];

// from r_things.c/h
extern int *negonearray; // [MAX_SCREENWIDTH];        // killough 2/8/98: // dropoff overflow
extern int *screenheightarray; // [MAX_SCREENWIDTH];  // change to MAX_* // dropoff overflow
extern spriteframe_t *sprtemp; // [MAX_SPRITE_FRAMES]; [29]

#if 0
extern Chip *opl_chip;

extern visplane_t **visplanes; // [MAXVISPLANES];   // killough
extern fixed_t *cachedheight; // [MAX_SCREENHEIGHT];
extern fixed_t *cacheddistance; // [MAX_SCREENHEIGHT];
extern fixed_t *cachedxstep; // [MAX_SCREENHEIGHT];
extern fixed_t *cachedystep; // [MAX_SCREENHEIGHT];
extern int *spanstart; // [MAX_SCREENHEIGHT];                // killough 2/8/98
extern int *floorclip; // [MAX_SCREENWIDTH];
extern int *ceilingclip; // [MAX_SCREENWIDTH];
extern fixed_t *yslope; // [MAX_SCREENHEIGHT];
extern fixed_t *distscale; // [MAX_SCREENWIDTH];

extern patchnum_t *hu_font; // font[HU_FONTSIZE];
extern patchnum_t *hu_font2; // [HU_FONTSIZE];
extern patchnum_t *hu_fontk; // [HU_FONTSIZE];//jff 3/7/98 added for graphic key indicators
extern patchnum_t *hu_msgbg; // [9];          //jff 2/26/98 add patches for message background

extern char *hud_coordstrx; // [ 32];
extern char *hud_coordstry; // [ 32];
extern char *hud_coordstrz; // [ 32];
extern char *hud_ammostr; // [ 80];
extern char *hud_healthstr; // [ 80];
extern char *hud_armorstr; // [ 80];
extern char *hud_weapstr; // [ 80];
extern char *hud_keysstr; // [ 80];
extern char *hud_gkeysstr; // [ 80]; //jff 3/7/98 add support for graphic key display
extern char *hud_monsecstr; // [ 80];
extern char *chatchars; // [QUEUESIZE]; [128]

extern hu_textline_t  *w_title;
extern hu_stext_t     *w_message;
extern hu_itext_t     *w_chat;
extern hu_itext_t     *w_inputbuffer; // [MAXPLAYERS];
extern hu_textline_t  *w_coordx; //jff 2/16/98 new coord widget for automap
extern hu_textline_t  *w_coordy; //jff 3/3/98 split coord widgets automap
extern hu_textline_t  *w_coordz; //jff 3/3/98 split coord widgets automap
extern hu_textline_t  *w_ammo;   //jff 2/16/98 new ammo widget for hud
extern hu_textline_t  *w_health; //jff 2/16/98 new health widget for hud
extern hu_textline_t  *w_armor;  //jff 2/16/98 new armor widget for hud
extern hu_textline_t  *w_weapon; //jff 2/16/98 new weapon widget for hud
extern hu_textline_t  *w_keys;   //jff 2/16/98 new keys widget for hud
extern hu_textline_t  *w_gkeys;  //jff 3/7/98 graphic keys widget for hud
extern hu_textline_t  *w_monsec; //jff 2/16/98 new kill/secret widget for hud
extern hu_mtext_t     *w_rtext;  //jff 2/26/98 text message refresh widget

#endif

void init_doom(const std::string& wad_filename, uint8_t *wad_data, size_t wad_data_size) {
    doom_wad_path = wad_filename;

    // needed for doom.cpp
    channels = (channel_t *)shared_malloc(sizeof(channel_t) * NUM_MIX_CHANNELS);
    sfx = (const doom_sfx_t **)shared_malloc(sizeof(doom_sfx_t*) * NUMSFX);
    mixbuffer = (uint16_t *)shared_malloc(sizeof(uint16_t) * AUDIO_BUFFER_LENGTH);

    // needed for dbopl
    #if ( DBOPL_WAVE == WAVE_HANDLER ) || ( DBOPL_WAVE == WAVE_TABLELOG )
    ExpTable = (Bit16u *)shared_malloc(sizeof(Bit16u) * 256);
    #endif
    #if ( DBOPL_WAVE == WAVE_HANDLER )
    SinTable = (Bit16u *)shared_malloc(sizeof(Bit16u) * 512);
    #endif
    WaveTable = (Bit16s *)shared_malloc(sizeof(Bit16s) * 8 * 512);
    #if ( DBOPL_WAVE == WAVE_TABLEMUL )
    MulTable = (Bit16u *)shared_malloc(sizeof(Bit16u) * 384);
    #endif
    KslTable = (Bit8u *)shared_malloc(sizeof(Bit8u) * 8 * 16);
    TremoloTable = (Bit8u *)shared_malloc(sizeof(Bit8u) * TREMOLO_TABLE);
    ChanOffsetTable = (Bit16u *)shared_malloc(sizeof(Bit16u) * 32);
    OpOffsetTable = (Bit16u *)shared_malloc(sizeof(Bit16u) * 64);

    // needed for doomstat / g_game
    players = (player_t *)shared_malloc(sizeof(player_t) * MAXPLAYERS);
    gamekeydown = (byte *)shared_malloc(sizeof(byte) * NUMKEYS);

    // needed for info
    mobjinfo = (mobjinfo_t *)shared_malloc(sizeof(mobjinfo_t) * NUMMOBJTYPES);
    memcpy(mobjinfo, init_mobjinfo, sizeof(mobjinfo_t) * NUMMOBJTYPES);

    // needed for m_cheat
    cheat = (struct cheat_s *)shared_malloc(init_cheat_bytes);
    memcpy(cheat, init_cheat, init_cheat_bytes);

    // needed for p_mobj.c
    itemrespawnque = (mapthing_t *)shared_malloc(sizeof(mapthing_t) * ITEMQUESIZE);
    itemrespawntime = (int *)shared_malloc(sizeof(int) * ITEMQUESIZE);

    // needed for sounds.c/h
    S_music = (musicinfo_t *)shared_malloc(s_music_bytes);
    memcpy(S_music, init_S_music, s_music_bytes);
    S_sfx = (sfxinfo_t *)shared_malloc(s_sfx_bytes);
    memcpy(S_sfx, init_S_sfx, s_sfx_bytes);
    S_sfx[sfx_chgun].link = &S_sfx[sfx_pistol];

    // needed for st_stuff.c/h
    shortnum = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * 10);
    keys = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * (NUMCARDS + 3));
    faces = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * ST_NUMFACES);
    arms = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * 6 * 2);

    // needed for r_things.c/h
    negonearray = (int *)shared_malloc(sizeof(int) * MAX_SCREENWIDTH); // killough 2/8/98: // dropoff overflow
    screenheightarray = (int *)shared_malloc(sizeof(int) * MAX_SCREENWIDTH); // change to MAX_* // dropoff overflow
    sprtemp = (spriteframe_t *)shared_malloc(sizeof(spriteframe_t) * 29); // [29]

#if 0
    opl_chip = (Chip *)shared_malloc(sizeof(Chip));

    visplanes = (visplane_t **)shared_malloc(sizeof(visplane_t *) * MAXVISPLANES);
    cachedheight = (fixed_t *)shared_malloc(sizeof(fixed_t) * MAX_SCREENHEIGHT);
    cacheddistance = (fixed_t *)shared_malloc(sizeof(fixed_t) * MAX_SCREENHEIGHT);
    cachedxstep = (fixed_t *)shared_malloc(sizeof(fixed_t) * MAX_SCREENHEIGHT);
    cachedystep = (fixed_t *)shared_malloc(sizeof(fixed_t) * MAX_SCREENHEIGHT);

    spanstart = (int *)shared_malloc(sizeof(int) * MAX_SCREENHEIGHT);
    floorclip = (int *)shared_malloc(sizeof(int) * MAX_SCREENWIDTH);
    ceilingclip = (int *)shared_malloc(sizeof(int) * MAX_SCREENWIDTH);
    yslope = (fixed_t *)shared_malloc(sizeof(fixed_t) * MAX_SCREENHEIGHT);
    distscale = (fixed_t *)shared_malloc(sizeof(fixed_t) * MAX_SCREENWIDTH);

    hu_font = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * HU_FONTSIZE);
    hu_font2 = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * HU_FONTSIZE);
    hu_fontk = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * HU_FONTSIZE);
    hu_msgbg = (patchnum_t *)shared_malloc(sizeof(patchnum_t) * 9);

    hud_coordstrx = (char *)shared_malloc(32);
    hud_coordstry = (char *)shared_malloc(32);
    hud_coordstrz = (char *)shared_malloc(32);
    hud_ammostr = (char *)shared_malloc(80);
    hud_healthstr = (char *)shared_malloc(80);
    hud_armorstr = (char *)shared_malloc(80);
    hud_weapstr = (char *)shared_malloc(80);
    hud_keysstr = (char *)shared_malloc(80);
    hud_gkeysstr = (char *)shared_malloc(80); //jff 3/7/98 add support for graphic key display
    hud_monsecstr = (char *)shared_malloc(80);
    chatchars = (char *)shared_malloc(128); // [128]

    w_title = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_message = (hu_stext_t *)shared_malloc(sizeof(hu_stext_t));
    w_chat = (hu_itext_t *)shared_malloc(sizeof(hu_itext_t));
    w_inputbuffer = (hu_itext_t *)shared_malloc(sizeof(hu_itext_t) * MAXPLAYERS);
    w_coordx = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_coordy = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_coordz = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_ammo = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_health = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_armor = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_weapon = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_keys = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_gkeys = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t)); //jff 3/7/98 graphic keys widget for hud
    w_monsec = (hu_textline_t *)shared_malloc(sizeof(hu_textline_t));
    w_rtext = (hu_mtext_t *)shared_malloc(sizeof(hu_mtext_t));

#endif

    SCREENWIDTH = MAX_SCREENWIDTH;
    SCREENHEIGHT = MAX_SCREENHEIGHT;

    fmt::print("Loading WAD: {}\n", wad_filename);

    myargv = doom_argv;
    myargc = 5;
    doom_argv[0] = "doom";
    doom_argv[1] = "-save";
    doom_argv[2] = "/sdcard/doom";
    doom_argv[3] = "-iwad";
    doom_argv[4] = wad_filename.c_str();
    // doom_argv[5] = "-file";
    // doom_argv[6] = pwad;
    doom_argv[myargc] = 0;

    // Some things might be nice to place in internal RAM, but I do not have time to find such
    // structures. So for now, prefer external RAM for most things except the framebuffer which
    // is allocated above.
    heap_caps_malloc_extmem_enable(0);

    // Initialize game
    Z_Init();
    D_DoomMainSetup();

    fmt::print("Loaded WAD: {}\n", wad_filename);

    reset_frame_time();
}

void run_doom_rom() {
    auto start = esp_timer_get_time();

    // Update game state
    // From:
    // D_DoomLoop();

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
    // static constexpr uint64_t max_frame_time = 1000000 / 60;
    // if (!unlock && elapsed < max_frame_time) {
    //     auto sleep_time = (max_frame_time - elapsed) / 1e3;
    //     std::this_thread::sleep_for(sleep_time * std::chrono::milliseconds(1));
    // }
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
    size_t num_pixels = SCREENWIDTH * SCREENHEIGHT;
    std::vector<uint8_t> frame(num_pixels * sizeof(uint16_t));
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
    Z_FreeTags(0, PU_MAX);
    // reset audio state
    BoxEmu::get().audio_sample_rate(48000);
    shared_mem_clear();
}
