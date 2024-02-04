#include "genesis.hpp"

extern "C" {
/* Gwenesis Emulator */
#include "m68k.h"
#include "z80inst.h"
#include "ym2612.h"
#include "gwenesis_bus.h"
#include "gwenesis_io.h"
#include "gwenesis_vdp.h"
#include "gwenesis_savestate.h"
#include "gwenesis_sn76489.h"
};

#include <string>

#include "box_emu_hal.hpp"

static constexpr size_t GENESIS_SCREEN_WIDTH = 320;
static constexpr size_t GENESIS_VISIBLE_HEIGHT = 224;

// static uint16_t palette[PALETTE_SIZE];

static uint8_t *genesis_sram = nullptr;
static uint8_t *genesis_ram = nullptr;
static uint8_t *genesis_vdp_vram = nullptr;

static int frame_buffer_offset = 0;
static int frame_buffer_size = GENESIS_SCREEN_WIDTH * GENESIS_VISIBLE_HEIGHT;

static int frame_counter = 0;
static uint16_t muteFrameCount = 0;
static int frame_buffer_index = 0;

void genesis_frame(int skip_render);
void genesis_init(void);
void genesis_reset(void);

/// BEGIN GWENESIS EMULATOR

int16_t *gwenesis_sn76489_buffer;
int sn76489_index;
int sn76489_clock;
int16_t *gwenesis_ym2612_buffer;
int ym2612_index;
int ym2612_clock;

uint8_t *M68K_RAM = nullptr; // MAX_RAM_SIZE
uint8_t *ZRAM = nullptr; // MAX_Z80_RAM_SIZE

typedef struct {
    char key[28];
    uint32_t length;
} svar_t;

extern "C" SaveState* saveGwenesisStateOpenForRead(const char* fileName)
{
    return (SaveState*)1;
}

extern "C" SaveState* saveGwenesisStateOpenForWrite(const char* fileName)
{
    return (SaveState*)1;
}

extern "C" int saveGwenesisStateGet(SaveState* state, const char* tagName)
{
    int value = 0;
    saveGwenesisStateGetBuffer(state, tagName, &value, sizeof(int));
    return value;
}

extern "C" void saveGwenesisStateSet(SaveState* state, const char* tagName, int value)
{
    saveGwenesisStateSetBuffer(state, tagName, &value, sizeof(int));
}

static FILE* savestate_fp = nullptr;
static int savestate_errors = 0;
extern "C" void saveGwenesisStateGetBuffer(SaveState* state, const char* tagName, void* buffer, int length)
{
    size_t initial_pos = ftell(savestate_fp);
    bool from_start = false;
    svar_t var;

    // Odds are that calls to this func will be in order, so try searching from current file position.
    while (!from_start || ftell(savestate_fp) < initial_pos)
    {
        if (!fread(&var, sizeof(svar_t), 1, savestate_fp))
        {
            if (!from_start)
            {
                fseek(savestate_fp, 0, SEEK_SET);
                from_start = true;
                continue;
            }
            break;
        }
        if (strncmp(var.key, tagName, sizeof(var.key)) == 0)
        {
            fread(buffer, std::min<int>(var.length, length), 1, savestate_fp);
            fmt::print("Loaded key '{}'\n", tagName);
            return;
        }
        fseek(savestate_fp, var.length, SEEK_CUR);
    }
    fmt::print("Key {} NOT FOUND!\n", tagName);
    savestate_errors++;
}

extern "C" void saveGwenesisStateSetBuffer(SaveState* state, const char* tagName, void* buffer, int length)
{
    // TO DO: seek the file to find if the key already exists. It's possible it could be written twice.
    svar_t var = {{0}, (uint32_t)length};
    strncpy(var.key, tagName, sizeof(var.key) - 1);
    // fwrite(&var, sizeof(var), 1, savestate_fp);
    // fwrite(buffer, length, 1, savestate_fp);
    fmt::print("Saved key '{}'\n", tagName);
}

extern "C" void gwenesis_io_get_buttons()
{
}

/// END GWENESIS EMULATOR

void reset_genesis() {
  reset_emulation();
}

static void init(uint8_t *romdata, size_t rom_data_size) {
  static bool initialized = false;
  if (!initialized) {
    // genesis_sram = (uint8_t*)heap_caps_malloc(0x8000, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    // genesis_ram = (uint8_t*)heap_caps_malloc(0x2000, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    // genesis_vdp_vram = (uint8_t*)heap_caps_malloc(0x4000, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  }

  // bitmap.width = GENESIS_SCREEN_WIDTH;
  // bitmap.height = GENESIS_VISIBLE_HEIGHT;
  // bitmap.pitch = bitmap.width;
  // bitmap.data = hal::get_frame_buffer0();

  // cart.sram = genesis_sram;
  // genesis.wram = genesis_ram;
  // genesis.use_fm = 0;
  // vdp.vram = genesis_vdp_vram;

  // set_option_defaults();

  // option.sndrate = hal::AUDIO_SAMPLE_RATE;
  // option.overscan = 0;

  // system_init2();
  // system_reset();

  // frame_counter = 0;
  // muteFrameCount = 0;

  initialized = true;
  reset_frame_time();
}

void init_genesis(uint8_t *romdata, size_t rom_data_size) {
  hal::set_native_size(GENESIS_SCREEN_WIDTH, GENESIS_VISIBLE_HEIGHT, GENESIS_SCREEN_WIDTH);
  // load_rom_data(romdata, rom_data_size);
  // genesis.console = CONSOLE_GENESIS;
  // genesis.display = DISPLAY_NTSC;
  // genesis.territory = TERRITORY_DOMESTIC;
  // frame_buffer_offset = 0;
  // frame_buffer_size = GENESIS_SCREEN_WIDTH * GENESIS_VISIBLE_HEIGHT;
  init(romdata, rom_data_size);
  fmt::print("genesis init done\n");
}

void run_genesis_rom() {
  auto start = std::chrono::high_resolution_clock::now();
  // handle input here (see system.h and use input.pad and input.system)
  InputState state;
  hal::get_input_state(&state);

  // // pad[0] is player 0
  // input.pad[0] = 0;
  // input.pad[0]|= state.up ? INPUT_UP : 0;
  // input.pad[0]|= state.down ? INPUT_DOWN : 0;
  // input.pad[0]|= state.left ? INPUT_LEFT : 0;
  // input.pad[0]|= state.right ? INPUT_RIGHT : 0;
  // input.pad[0]|= state.a ? INPUT_BUTTON2 : 0;
  // input.pad[0]|= state.b ? INPUT_BUTTON1 : 0;

  // // pad[1] is player 1
  // input.pad[1] = 0;

  // // system is where we input the start button, as well as soft reset
  // input.system = 0;
  // input.system |= state.start ? INPUT_START : 0;
  // input.system |= state.select ? INPUT_PAUSE : 0;

  // emulate the frame
  // if (0 || (frame_counter % 2) == 0) {
  //   memset(bitmap.data, 0, frame_buffer_size);
  //   system_frame(0);

  //   // copy the palette
  //   render_copy_palette(palette);
  //   // flip the bytes in the palette
  //   for (int i = 0; i < PALETTE_SIZE; i++) {
  //     uint16_t rgb565 = palette[i];
  //     palette[i] = (rgb565 >> 8) | (rgb565 << 8);
  //   }
  //   // set the palette
  //   hal::set_palette(palette, PALETTE_SIZE);

  //   // render the frame
  //   hal::push_frame((uint8_t*)bitmap.data + frame_buffer_offset);
  //   // ping pong the frame buffer
  //   frame_buffer_index = !frame_buffer_index;
  //   bitmap.data = frame_buffer_index
  //     ? (uint8_t*)hal::get_frame_buffer1()
  //     : (uint8_t*)hal::get_frame_buffer0();
  // } else {
  //   system_frame(1);
  // }

  // ++frame_counter;

  // // Process audio
  // int16_t *audio_buffer = (int16_t*)hal::get_audio_buffer();
  // for (int x = 0; x < genesis_snd.sample_count; x++) {
  //   uint32_t sample;

  //   if (muteFrameCount < 60 * 2) {
  //     // When the emulator starts, audible poping is generated.
  //     // Audio should be disabled during this startup period.
  //     sample = 0;
  //     ++muteFrameCount;
  //   } else {
  //     sample = (genesis_snd.output[0][x] << 16) + genesis_snd.output[1][x];
  //   }

  //   audio_buffer[x] = sample;
  // }
  // auto audio_buffer_len = genesis_snd.sample_count - 1;

  // // push the audio buffer to the audio task
  // hal::play_audio((uint8_t*)audio_buffer, audio_buffer_len * 2);

  // manage statistics
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration<float>(end-start).count();
  update_frame_time(elapsed);
}

void load_genesis(std::string_view save_path) {
  if (save_path.size()) {
    auto f = fopen(save_path.data(), "rb");
    // system_load_state(f);
    fclose(f);
  }
}

void save_genesis(std::string_view save_path) {
  // open the save path as a file descriptor
  auto f = fopen(save_path.data(), "wb");
  // system_save_state(f);
  fclose(f);
}

std::vector<uint8_t> get_genesis_video_buffer() {
  int height = GENESIS_VISIBLE_HEIGHT;
  int width = GENESIS_SCREEN_WIDTH;
  int pitch = GENESIS_SCREEN_WIDTH;
  std::vector<uint8_t> frame(width * height * 2);
  // the frame data for the GENESIS is stored in bitmap.data as a 8 bit index into
  // the palette we need to convert this to a 16 bit RGB565 value
  // const uint8_t *frame_buffer = bitmap.data + frame_buffer_offset;
  // for (int y = 0; y < height; y++) {
  //   for (int x = 0; x < width; x++) {
  //     uint8_t index = frame_buffer[y * pitch + x];
  //     uint16_t rgb565 = palette[index % PALETTE_SIZE];
  //     frame[(y * width + x)*2] = rgb565 & 0xFF;
  //     frame[(y * width + x)*2+1] = (rgb565 >> 8) & 0xFF;
  //   }
  // }
  return frame;
}

void deinit_genesis() {
  // TODO:
}
