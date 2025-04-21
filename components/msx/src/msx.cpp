#include "msx.hpp"

#include "shared_memory.h"

#include <memory>

#include "box-emu.hpp"
#include "statistics.hpp"

static const size_t MSX_SCREEN_WIDTH = 256;
static const size_t MSX_SCREEN_HEIGHT = 228;
static const int MSX_AUDIO_SAMPLE_RATE = 32000;

extern "C" {

#define AUDIO_SAMPLE_RATE (32000)
#define AUDIO_BUFFER_LENGTH (AUDIO_SAMPLE_RATE / 60 + 1)

static uint16_t* displayBuffer[2];
static uint8_t currentBuffer = 0;
static uint16_t* framebuffer = nullptr;
static uint16_t* audio_buffers[2];
static uint8_t currentAudioBuffer = 0;
static uint16_t* audio_buffer = nullptr;
static std::atomic<bool> frame_complete = false;

static bool unlock = false;

static int JoyState, LastKey, InMenu, InKeyboard;
static int KeyboardCol, KeyboardRow, KeyboardKey;
static int64_t KeyboardDebounce = 0;
static int KeyboardEmulation, CropPicture;
static char *PendingLoadSTA = NULL;

// #define BPS16
#define BPP16
// #define UNIX
#define GenericSetVideo SetVideo
// #define LSB_FIRST
// #define NARROW
#define WIDTH 256
#define HEIGHT 228
#define XKEYS 12
#define YKEYS 6

void PutImage(void);

uint16_t *BPal; // [256];
uint16_t *XPal; // [80];
uint16_t XPal0;
uint16_t *XBuf;


#include "MSX.h"
#include "Console.h"
#include "EMULib.h"
#include "Sound.h"
#include "Record.h"
#include "Touch.h"
#include "CommonMux.h"
#include "msxfix.h"

#include "Floppy.h"
#include "MCF.h"
#include "Hunt.h"
};

static Image *NormScreen;
const char *Title = "fMSX 6.0";
const char *Disks[2][MAXDISKS + 1];

static const unsigned char KBDKeys[YKEYS][XKEYS] = {
    {0x1B, CON_F1, CON_F2, CON_F3, CON_F4, CON_F5, CON_F6, CON_F7, CON_F8, CON_INSERT, CON_DELETE, CON_STOP},
    {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '='},
    {CON_TAB, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', CON_BS},
    {'^', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', CON_ENTER},
    {'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', 0, 0},
    {'[', ']', ' ', ' ', ' ', ' ', ' ', '\\', '\'', 0, 0, 0}};

static const char *BiosFolder = "/sdcard/msx/bios/";
// We only check for absolutely essential files to avoid slowing down boot too much!
static const char *BiosFiles[] = {
    "MSX.ROM",
    "MSX2.ROM",
    "MSX2EXT.ROM",
    // "MSX2P.ROM",
    // "MSX2PEXT.ROM",
    // "FMPAC.ROM",
    "DISK.ROM",
    "MSXDOS2.ROM",
    // "PAINTER.ROM",
    // "KANJI.ROM",
};

bool wait_for_key(GamepadState::Button key, bool state, int timeout_ms) {
    static auto& box = BoxEmu::get();
    auto buttons = box.gamepad_state();
    auto start = esp_timer_get_time();
    auto timeout = start + (timeout_ms * 1000);
    using namespace std::chrono_literals;
    while (buttons.is_pressed(key) != state) {
        if (esp_timer_get_time() > timeout) {
            return false;
        }
        std::this_thread::sleep_for(10ms);
        buttons = box.gamepad_state();
    }
    return true;
}

int ProcessEvents(int Wait)
{
    for (int i = 0; i < 16; ++i)
        KeyState[i] = 0xFF;
    JoyState = 0;

    static auto& box = BoxEmu::get();
    auto state = box.gamepad_state();

    // update unlock based on x button
    static bool last_x = false;
    if (state.x && !last_x) {
        unlock = !unlock;
    }
    last_x = state.x;

    if (state.select)
    {
        InKeyboard = !InKeyboard;
        // wait no more than 500ms for the user to release the key
        wait_for_key(GamepadState::Button::SELECT, false, 500);
    }
    else if (state.start)
    {
        // I think this key could be better used for something else
        // but for now the feedback is to keep a key for fMSX menu...
        InMenu = 2;
        return 0;
    }

    if (InMenu == 2)
    {
        InMenu = 1;
        // mute audio
        bool is_muted = box.is_muted();
        box.mute(true);
        MenuMSX();
        // unmute audio
        box.mute(is_muted);
        // wait at least 500ms for the user to release the key
        wait_for_key(GamepadState::Button::START, false, 500);
        InMenu = 0;
    }
    else if (InMenu)
    {
        if (state.left)
            LastKey = CON_LEFT;
        if (state.right)
            LastKey = CON_RIGHT;
        if (state.up)
            LastKey = CON_UP;
        if (state.down)
            LastKey = CON_DOWN;
        if (state.a)
            LastKey = CON_OK;
        if (state.b)
            LastKey = CON_EXIT;
    }
    else if (InKeyboard)
    {
        if (state.up || state.down || state.left || state.right)
        {
            if (esp_timer_get_time() > KeyboardDebounce)
            {
                if (state.left)
                    KeyboardCol--;
                if (state.right)
                    KeyboardCol++;
                if (state.up)
                    KeyboardRow--;
                if (state.down)
                    KeyboardRow++;

                KeyboardCol = std::clamp(KeyboardCol, 0, XKEYS - 1);
                KeyboardRow = std::clamp(KeyboardRow, 0, YKEYS - 1);
                PutImage();
                KeyboardDebounce = esp_timer_get_time() + 250'000; // 250ms
            }
        }
        else if (state.a)
        {
            KeyboardKey = KBDKeys[KeyboardRow][KeyboardCol];
            KBD_SET(KeyboardKey);
        }
        else if (state.b)
        {
            // wait at least 500ms for the user to release the key
            wait_for_key(GamepadState::Button::B, false, 500);
            InKeyboard = false;
        }
    }
    else if (KeyboardEmulation)
    {
        if (state.left)
            KBD_SET(KBD_LEFT);
        if (state.right)
            KBD_SET(KBD_RIGHT);
        if (state.up)
            KBD_SET(KBD_UP);
        if (state.down)
            KBD_SET(KBD_DOWN);
        if (state.a)
            KBD_SET(KBD_SPACE);
        if (state.b)
            KBD_SET(KBD_ENTER);
    }
    else
    {
        if (state.left)
            JoyState |= JST_LEFT;
        if (state.right)
            JoyState |= JST_RIGHT;
        if (state.up)
            JoyState |= JST_UP;
        if (state.down)
            JoyState |= JST_DOWN;
        if (state.a)
            JoyState |= JST_FIREA;
        if (state.b)
            JoyState |= JST_FIREB;
    }

    return 0;
}

int InitMachine(void)
{
    *NormScreen = (Image){
        .Data = framebuffer,
        .W = WIDTH,
        .H = HEIGHT,
        .L = WIDTH,
        .D = 16,
    };

    XBuf = NormScreen->Data;
    SetScreenDepth(NormScreen->D);
    SetVideo(NormScreen, 0, 0, WIDTH, HEIGHT);

    for (int J = 0; J < 80; J++)
        SetColor(J, 0, 0, 0);

    for (int J = 0; J < 256; J++)
    {
        uint16_t color = make_color(((J >> 2) & 0x07) * 255 / 7, ((J >> 5) & 0x07) * 255 / 7, (J & 0x03) * 255 / 3);
        // BPal[J] = ((color >> 8) | (color << 8)) & 0xFFFF;
        BPal[J] = color;
    }

    InitSound(AUDIO_SAMPLE_RATE, 150);
    SetChannels(64, 0xFFFFFFFF);

    RPLInit(SaveState, LoadState, MAX_STASIZE);
    RPLRecord(RPL_RESET);
    return 1;
}

void TrashMachine(void)
{
    RPLTrash();
    TrashSound();
}

void SetColor(byte N, byte R, byte G, byte B)
{
    uint16_t color = make_color(R, G, B);
    // color = (color >> 8) | (color << 8);
    color = color;
    if (N)
        XPal[N] = color;
    else
        XPal0 = color;
}

static inline void SubmitFrame(void)
{
    static auto& box = BoxEmu::get();
    box.push_frame(framebuffer);

    // swap buffers
    currentBuffer = currentBuffer ? 0 : 1;
    framebuffer = displayBuffer[currentBuffer];
    NormScreen->Data = framebuffer;
    XBuf = NormScreen->Data;

    frame_complete = true;
}

void PutImage(void)
{
    if (InKeyboard)
        DrawKeyboard(NormScreen, KBDKeys[KeyboardRow][KeyboardCol]);

    SubmitFrame();

    XBuf = NormScreen->Data;
}

unsigned int Joystick(void)
{
    ProcessEvents(0);
    return JoyState;
}

void Keyboard(void)
{
    // Keyboard() is a convenient place to do our vsync stuff :)
    if (PendingLoadSTA)
    {
        LoadSTA(PendingLoadSTA);
        free(PendingLoadSTA);
        PendingLoadSTA = NULL;
    }
}

unsigned int Mouse(byte N)
{
    return 0;
}

int ShowVideo(void)
{
    SubmitFrame();
    return 1;
}

unsigned int GetJoystick(void)
{
    ProcessEvents(0);
    return 0;
}

unsigned int GetMouse(void)
{
    return 0;
}

unsigned int GetKey(void)
{
    unsigned int J;
    ProcessEvents(0);
    J = LastKey;
    LastKey = 0;
    return J;
}

unsigned int WaitKey(void)
{
    LastKey = 0;
    // wait no more than 200ms for the user to release the key
    wait_for_key(GamepadState::Button::ANY, false, 200);
    // wait no more than 100ms for the user to press any key
    while (!wait_for_key(GamepadState::Button::ANY, true, 100)) {
        // wait for key
        continue;
    }

    return GetKey();
}

unsigned int WaitKeyOrMouse(void)
{
    LastKey = WaitKey();
    return 0;
}

static size_t audio_buffer_offset = 0;
unsigned int InitAudio(unsigned int Rate, unsigned int Latency)
{
    BoxEmu::get().audio_sample_rate(Rate);
    return AUDIO_SAMPLE_RATE;
}

void TrashAudio(void)
{
    //
}

unsigned int GetFreeAudio(void)
{
    return 1024;
}

void PlayAllSound(int uSec) {
    static auto &box = BoxEmu::get();
    unsigned int samples = 2 * uSec * AUDIO_SAMPLE_RATE / 1'000'000;
    RenderAndPlayAudio(samples);
}

unsigned int WriteAudio(sample *Data, unsigned int Length) {
    static auto &box = BoxEmu::get();
    bool sound_enabled = !box.is_muted();
    if (sound_enabled) {
        if (audio_buffer_offset + Length > AUDIO_BUFFER_LENGTH) {
            box.play_audio((uint8_t*)audio_buffer, audio_buffer_offset * sizeof(int16_t));
            audio_buffer_offset = 0;
            currentAudioBuffer = currentAudioBuffer ? 0 : 1;
            audio_buffer = audio_buffers[currentAudioBuffer];
        }
        memcpy(audio_buffer + audio_buffer_offset, Data, Length * sizeof(int16_t));
        audio_buffer_offset += Length;
    }
    return Length;
}

void reset_msx() {
  ResetMSX(Mode,RAMPages,VRAMPages);
}

extern I8255* PPI;
extern Z80* CPU;
extern void **Chunks; // MAXCHUNKS
extern WD1793* FDC;
extern FDIDisk* FDD; // 4
extern AY8910* PSG;
extern YM2413* OPLL;
extern SCC* SCChip;
extern I8251* SIO;
extern MCFEntry *MCFEntries; // MAXCHEATS
extern CheatCode *CheatCodes; // MAXCHEATS
extern HUNTEntry *Buf;
extern RPLState *RPLData; // [RPL_BUFSIZE] = {{0}};

extern char *msx_path_buffer; // [100];
extern char *msx_path_cwd; // [100] = "/sdcard/";

void init_msx(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size) {
    // init shared memory
    msx_path_buffer = (char*)shared_malloc(100);
    msx_path_cwd = (char*)shared_malloc(100);
    snprintf(msx_path_cwd, strlen("/sdcard/") + 1, "%s", "/sdcard/");
    BPal = (uint16_t*)shared_malloc(256 * sizeof(uint16_t));
    XPal = (uint16_t*)shared_malloc(80 * sizeof(uint16_t));
    NormScreen = (Image*)shared_malloc(sizeof(Image));
    CPU = (Z80*)shared_malloc(sizeof(Z80));
    Chunks = (void**)shared_malloc(sizeof(void*) * MAXCHUNKS);
    PPI = (I8255*)shared_malloc(sizeof(I8255));
    FDC = (WD1793*)shared_malloc(sizeof(WD1793));
    FDD = (FDIDisk*)shared_malloc(sizeof(FDIDisk) * 4);
    PSG = (AY8910*)shared_malloc(sizeof(AY8910));
    OPLL = (YM2413*)shared_malloc(sizeof(YM2413));
    SCChip= (SCC*)shared_malloc(sizeof(SCC));
    SIO = (I8251*)shared_malloc(sizeof(I8251));
    MCFEntries = (MCFEntry*)shared_malloc(sizeof(MCFEntry) * MAXCHEATS);
    CheatCodes = (CheatCode*)shared_malloc(sizeof(CheatCode) * MAXCHEATS);
    Buf = (HUNTEntry*)shared_malloc(sizeof(HUNTEntry) * HUNT_BUFSIZE);
    RPLData = (RPLState*)shared_malloc(sizeof(RPLState) * RPL_BUFSIZE);

  // reset unlock
  unlock = false;

    // now init the state
  displayBuffer[0] = (uint16_t*)BoxEmu::get().frame_buffer0();
  displayBuffer[1] = (uint16_t*)BoxEmu::get().frame_buffer1();
  currentBuffer = 0;
  framebuffer = displayBuffer[0];

  audio_buffers[0] = (uint16_t*)shared_malloc(AUDIO_BUFFER_LENGTH * sizeof(int16_t));
  audio_buffers[1] = (uint16_t*)shared_malloc(AUDIO_BUFFER_LENGTH * sizeof(int16_t));
  currentAudioBuffer = 0;
  audio_buffer = audio_buffers[currentAudioBuffer];

  BoxEmu::get().native_size(MSX_SCREEN_WIDTH, MSX_SCREEN_HEIGHT);
  BoxEmu::get().palette(nullptr);

  reset_frame_time();

  const char *argv[] = {
      "fmsx",
      "-ram", "2",
      "-vram", "2",
      "-skip", "50",
      "-home", BiosFolder,
      "-joy", "1",
      NULL, NULL, NULL,
  };
  int argc = std::size(argv) - 3;

  // get the file extension from the rom_filename
  const char *ext = strrchr(rom_filename.c_str(), '.');
  if (ext != NULL) {
      ext++;
      if (strcasecmp(ext, "dsk") == 0) {
          argv[argc++] = "-diska";
      }
  }

  argv[argc++] = rom_filename.c_str();

  // started
  fmsx_main(argc, (char **)argv);

}

void IRAM_ATTR run_msx_rom() {
  auto start = esp_timer_get_time();

  // static constexpr size_t num_cycles_per_frame = 2000;
  // for (int i = 0; i < num_cycles_per_frame; i++) {
  while (!frame_complete) {
      RunZ80(CPU);
  }
  frame_complete = false;

  auto end = esp_timer_get_time();
  auto elapsed = end - start;
  update_frame_time(elapsed);

  static constexpr uint64_t max_frame_time = 1000000 / 60;
  if (!unlock && elapsed < max_frame_time) {
      using namespace std::chrono_literals;
    auto sleep_time = (max_frame_time - elapsed) / 1e3;
    std::this_thread::sleep_for(sleep_time * 1ms);
  }
}

void load_msx(std::string_view save_path) {
  if (PendingLoadSTA)
    free(PendingLoadSTA);
  PendingLoadSTA = strdup(save_path.data());
}

void save_msx(std::string_view save_path) {
  // save_sram((char *)save_path.data(), console_msx);
  SaveSTA((char *)save_path.data());
}

std::vector<uint8_t> get_msx_video_buffer() {
  // copy the frame buffer to a new buffer
  static constexpr auto width = MSX_SCREEN_WIDTH;
  static constexpr auto height = MSX_SCREEN_HEIGHT;
  std::vector<uint8_t> new_frame_buffer(width * 2 * height);
  memcpy(new_frame_buffer.data(), framebuffer, width * 2 * height);
  return new_frame_buffer;
}

void deinit_msx() {
    TrashMSX();
    TrashMachine();
    RPLTrash();
    TrashSound();
    shared_mem_clear();
    BoxEmu::get().audio_sample_rate(48000);
}
