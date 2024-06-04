#include "genesis.hpp"

#pragma GCC optimize("Ofast")

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

static constexpr int AUDIO_BUFFER_LENGTH = std::max(GWENESIS_AUDIO_BUFFER_LENGTH_NTSC, GWENESIS_AUDIO_BUFFER_LENGTH_PAL);

static constexpr size_t GENESIS_SCREEN_WIDTH = 320;
static constexpr size_t GENESIS_VISIBLE_HEIGHT = 224;

static constexpr size_t PALETTE_SIZE = 256;
static uint16_t palette[PALETTE_SIZE];

static int frame_counter = 0;
static uint16_t muteFrameCount = 0;
static int frame_buffer_index = 0;
static uint8_t *frame_buffer = nullptr;

/// BEGIN GWENESIS EMULATOR

extern unsigned char* VRAM;
extern int zclk;
int system_clock;
int scan_line;

int16_t *gwenesis_sn76489_buffer = nullptr;
int sn76489_index;
int sn76489_clock;
int16_t *gwenesis_ym2612_buffer = nullptr;
int ym2612_index;
int ym2612_clock;

static int frameskip = 3;

static FILE *savestate_fp = NULL;
static int savestate_errors = 0;

uint8_t *M68K_RAM = nullptr; // MAX_RAM_SIZE
uint8_t *ZRAM = nullptr; // MAX_Z80_RAM_SIZE

int32_t *lfo_pm_table = nullptr; // 128*8*32

signed int *tl_tab = nullptr; // 13*2*TL_RES_LEN (13*2*256)

extern unsigned char gwenesis_vdp_regs[0x20];
extern unsigned int gwenesis_vdp_status;
extern unsigned short CRAM565[256];
extern unsigned int screen_width, screen_height;
extern int hint_pending;

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
            // fmt::print("Loaded key '{}'\n", tagName);
            return;
        }
        fseek(savestate_fp, var.length, SEEK_CUR);
    }
    fmt::print("Key {} NOT FOUND!\n", tagName);
    savestate_errors++;
}

extern "C" void saveGwenesisStateSetBuffer(SaveState* state, const char* tagName, const void* buffer, int length)
{
    // TO DO: seek the file to find if the key already exists. It's possible it could be written twice.
    svar_t var = {{0}, (uint32_t)length};
    strncpy(var.key, tagName, sizeof(var.key) - 1);
    fwrite(&var, sizeof(var), 1, savestate_fp);
    fwrite(buffer, length, 1, savestate_fp);
    // fmt::print("Saved key '{}'\n", tagName);
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
    VRAM = (uint8_t*)heap_caps_malloc(VRAM_MAX_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    gwenesis_sn76489_buffer = (int16_t*)heap_caps_malloc(AUDIO_BUFFER_LENGTH * sizeof(int16_t), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    gwenesis_ym2612_buffer = (int16_t*)heap_caps_malloc(AUDIO_BUFFER_LENGTH * sizeof(int16_t), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    M68K_RAM = (uint8_t*)heap_caps_malloc(MAX_RAM_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    ZRAM = (uint8_t*)heap_caps_malloc(MAX_Z80_RAM_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    lfo_pm_table = (int32_t*)heap_caps_malloc(128*8*32 * sizeof(int32_t), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    tl_tab = (signed int*)heap_caps_malloc(13*2*256 * sizeof(signed int), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  }

  load_cartridge(romdata, rom_data_size);

  power_on();

  reset_genesis();

  frame_counter = 0;
  muteFrameCount = 0;

  hal::set_audio_sample_rate(REG1_PAL ? GWENESIS_AUDIO_FREQ_PAL/2 : GWENESIS_AUDIO_FREQ_NTSC/2);

  frame_buffer = frame_buffer_index
    ? hal::get_frame_buffer1()
    : hal::get_frame_buffer0();
  gwenesis_vdp_set_buffer(frame_buffer);

  initialized = true;
  reset_frame_time();
}

void init_genesis(uint8_t *romdata, size_t rom_data_size) {
  hal::set_native_size(GENESIS_SCREEN_WIDTH, GENESIS_VISIBLE_HEIGHT, GENESIS_SCREEN_WIDTH);
  init(romdata, rom_data_size);
}

void IRAM_ATTR run_genesis_rom() {
  auto start = esp_timer_get_time();
  // handle input here (see system.h and use input.pad and input.system)
  static InputState previous_state = {};
  InputState state = {};
  hal::get_input_state(&state);

  // set frameskip to be 3 if muted, 60 otherwise
  frameskip = 3; // hal::is_muted() ? 3 : 60;

  if (previous_state != state) {
    // button mapping:
    // up, down, left, right, c, b, a, start
    // from gwenesis/src/io/gwenesis_io.c
    const bool keys[] = {
      (bool)state.up,
      (bool)state.down,
      (bool)state.left,
      (bool)state.right,
      (bool)state.y, // in genesis, C is the far right, so we'll make it Y
      (bool)state.a, // in genesis, B is the middle, so we'll make it A
      (bool)state.b, // in genesis, A is the far left, so we'll make it B
      (bool)state.start,
    };

    for (int i=0; i<sizeof(keys); i++) {
      if (keys[i]) {
        gwenesis_io_pad_press_button(0, i);
      } else {
        gwenesis_io_pad_release_button(0, i);
      }
    }
  }

  previous_state = state;

  bool drawFrame = (frame_counter++ % frameskip) == 0;

  int lines_per_frame = REG1_PAL ? LINES_PER_FRAME_PAL : LINES_PER_FRAME_NTSC;
  int hint_counter = gwenesis_vdp_regs[10];

  screen_width = REG12_MODE_H40 ? 320 : 256;
  screen_height = REG1_PAL ? 240 : 224;

  gwenesis_vdp_render_config();

  bool sound_enabled = !hal::is_muted();

  /* Reset the difference clocks and audio index */
  system_clock = 0;
  zclk = sound_enabled ? 0 : 0x1000000;

  ym2612_clock = sound_enabled ? 0 : 0x1000000;
  ym2612_index = 0;

  sn76489_clock = sound_enabled ? 0 : 0x1000000;
  sn76489_index = 0;

  scan_line = 0;

  int _vdp_cycles_per_line = VDP_CYCLES_PER_LINE / 2;

  while (scan_line < lines_per_frame) {
    system_clock += _vdp_cycles_per_line;

    m68k_run(system_clock);
    z80_run(system_clock);

    /* Audio */
    /*  GWENESIS_AUDIO_ACCURATE:
     *    =1 : cycle accurate mode. audio is refreshed when CPUs are performing a R/W access
     *    =0 : line  accurate mode. audio is refreshed every lines.
     */
    if (GWENESIS_AUDIO_ACCURATE == 0) {
      gwenesis_SN76489_run(system_clock);
      ym2612_run(system_clock);
    }

    /* Video */
    if (drawFrame && scan_line < screen_height)
      gwenesis_vdp_render_line(scan_line); /* render scan_line */

    // On these lines, the line counter interrupt is reloaded
    if ((scan_line == 0) || (scan_line > screen_height)) {
      //  if (REG0_LINE_INTERRUPT != 0)
      //    printf("HINTERRUPT counter reloaded: (scan_line: %d, new
      //    counter: %d)\n", scan_line, REG10_LINE_COUNTER);
      hint_counter = REG10_LINE_COUNTER;
    }

    // interrupt line counter
    if (--hint_counter < 0) {
      if ((REG0_LINE_INTERRUPT != 0) && (scan_line <= screen_height)) {
        hint_pending = 1;
        // printf("Line int pending %d\n",scan_line);
        if ((gwenesis_vdp_status & STATUS_VIRQPENDING) == 0)
          m68k_update_irq(4);
      }
      hint_counter = REG10_LINE_COUNTER;
    }

    scan_line++;

    // vblank begin at the end of last rendered line
    if (scan_line == screen_height) {
      if (REG1_VBLANK_INTERRUPT != 0) {
        gwenesis_vdp_status |= STATUS_VIRQPENDING;
        m68k_set_irq(6);
      }
      z80_irq_line(1);
    }
    if (scan_line == (screen_height + 1)) {
      z80_irq_line(0);
    }

  } // end of scanline loop

  /* Audio
   * synchronize YM2612 and SN76489 to system_clock
   * it completes the missing audio sample for accurate audio mode
   */
  if (GWENESIS_AUDIO_ACCURATE == 1) {
    gwenesis_SN76489_run(system_clock);
    ym2612_run(system_clock);
  }

  // reset m68k cycles to the begin of next frame cycle
  m68k.cycles -= system_clock;

  if (drawFrame) {
    // copy the palette
    memcpy(palette, CRAM565, PALETTE_SIZE * sizeof(uint16_t));
    // set the palette
    hal::set_palette(palette, PALETTE_SIZE);
    // push the frame buffer to the display task
    hal::push_frame(frame_buffer);
    // ping pong the frame buffer
    frame_buffer_index = !frame_buffer_index;
    frame_buffer = frame_buffer_index
      ? hal::get_frame_buffer1()
      : hal::get_frame_buffer0();
    gwenesis_vdp_set_buffer(frame_buffer);
  }

  if (sound_enabled) {
    // push the audio buffer to the audio task
    int audio_len = REG1_PAL ? GWENESIS_AUDIO_BUFFER_LENGTH_PAL : GWENESIS_AUDIO_BUFFER_LENGTH_NTSC;
    hal::play_audio((uint8_t*)gwenesis_ym2612_buffer, audio_len);
  }

  // manage statistics
  auto end = esp_timer_get_time();
  auto elapsed = end - start;
  update_frame_time(elapsed);
  std::this_thread::sleep_for(16ms - std::chrono::milliseconds(elapsed / 1e3));
}

void load_genesis(std::string_view save_path) {
  if (save_path.size()) {
    savestate_fp = fopen(save_path.data(), "rb");
    gwenesis_load_state();
    fclose(savestate_fp);
  }
}

void save_genesis(std::string_view save_path) {
  // open the save path as a file descriptor
  savestate_fp = fopen(save_path.data(), "wb");
  gwenesis_save_state();
  fclose(savestate_fp);
}

std::vector<uint8_t> get_genesis_video_buffer() {
  print_statistics();
  reset_frame_time();
  static constexpr int height = GENESIS_VISIBLE_HEIGHT;
  static constexpr int width = GENESIS_SCREEN_WIDTH;
  std::vector<uint8_t> frame(width * height * 2);
  // the frame data for genesis is stored in the frame buffer as 8 bit palette
  // indexes, so we need to convert it to 16 bit color
  const uint8_t *buffer = (const uint8_t*)frame_buffer;
  uint16_t *frame_ptr = (uint16_t*)frame.data();
  for (int i = 0; i < (height*width); i++) {
    uint8_t index = buffer[i];
    frame_ptr[i] = palette[index % PALETTE_SIZE];
  }
  return frame;
}

void deinit_genesis() {
  hal::set_audio_sample_rate(48000);
}
