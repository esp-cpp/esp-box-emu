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

static int frame_buffer_offset = 0;
static int frame_buffer_size = GENESIS_SCREEN_WIDTH * GENESIS_VISIBLE_HEIGHT;

static int frame_counter = 0;
static uint16_t muteFrameCount = 0;
static int frame_buffer_index = 0;

void genesis_frame(int skip_render);
void genesis_init(void);
void genesis_reset(void);

/// BEGIN GWENESIS EMULATOR

extern unsigned char* VRAM;
extern int zclk;
int system_clock;
int scan_line;

// TODO: replace
// static rg_video_update_t updates[2];
// static rg_video_update_t *currentUpdate = &updates[0];
// static rg_app_t *app;

static bool yfm_enabled = true;
static bool yfm_resample = true;
static bool z80_enabled = true;
static bool sn76489_enabled = true;
static int frameskip = 3;

static FILE *savestate_fp = NULL;
static int savestate_errors = 0;

int16_t *gwenesis_sn76489_buffer;
int sn76489_index;
int sn76489_clock;
int16_t *gwenesis_ym2612_buffer;
int ym2612_index;
int ym2612_clock;

uint8_t *M68K_RAM = nullptr; // MAX_RAM_SIZE
uint8_t *ZRAM = nullptr; // MAX_Z80_RAM_SIZE

uint32_t frames = 0;
extern unsigned char gwenesis_vdp_regs[0x20];
extern unsigned int gwenesis_vdp_status;
extern unsigned short CRAM565[256];
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
    VRAM = (uint8_t*)heap_caps_malloc(VRAM_MAX_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  }

  load_cartridge(romdata, rom_data_size);
  power_on();
  reset_genesis();

  frames = 0;
  // muteFrameCount = 0;

  initialized = true;
  reset_frame_time();
}

void init_genesis(uint8_t *romdata, size_t rom_data_size) {
  hal::set_native_size(GENESIS_SCREEN_WIDTH, GENESIS_VISIBLE_HEIGHT, GENESIS_SCREEN_WIDTH);
  frame_buffer_offset = 0;
  frame_buffer_size = GENESIS_SCREEN_WIDTH * GENESIS_VISIBLE_HEIGHT;
  init(romdata, rom_data_size);
  fmt::print("genesis init done\n");
}

void run_genesis_rom() {
  auto start = std::chrono::high_resolution_clock::now();
  // handle input here (see system.h and use input.pad and input.system)
  InputState state;
  hal::get_input_state(&state);

  bool keys[8] = {
    (bool)state.up,
    (bool)state.down,
    (bool)state.left,
    (bool)state.right,
    (bool)state.a,
    (bool)state.b,
    (bool)state.select,
    (bool)state.start
  };

  for (int i=0; i<8; i++) {
    if (keys[i]) {
      gwenesis_io_pad_press_button(0, i);
    } else {
      gwenesis_io_pad_release_button(0, i);
    }
  }

  bool drawFrame = (frames++ % frameskip) == 0;

  // TODO: swap these out
  // gwenesis_vdp_set_buffer(currentUpdate->buffer);
  gwenesis_vdp_render_config();

  int lines_per_frame = REG1_PAL ? LINES_PER_FRAME_PAL : LINES_PER_FRAME_NTSC;
  int hint_counter = gwenesis_vdp_regs[10];

  /* Reset the difference clocks and audio index */
  system_clock = 0;
  zclk = z80_enabled ? 0 : 0x1000000;

  ym2612_clock = yfm_enabled ? 0 : 0x1000000;
  ym2612_index = 0;

  sn76489_clock = sn76489_enabled ? 0 : 0x1000000;
  sn76489_index = 0;

  scan_line = 0;

  while (scan_line < lines_per_frame)
    {
      m68k_run(system_clock + VDP_CYCLES_PER_LINE);
      z80_run(system_clock + VDP_CYCLES_PER_LINE);

      /* Audio */
      /*  GWENESIS_AUDIO_ACCURATE:
       *    =1 : cycle accurate mode. audio is refreshed when CPUs are performing a R/W access
       *    =0 : line  accurate mode. audio is refreshed every lines.
       */
      if (GWENESIS_AUDIO_ACCURATE == 0) {
        gwenesis_SN76489_run(system_clock + VDP_CYCLES_PER_LINE);
        ym2612_run(system_clock + VDP_CYCLES_PER_LINE);
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

      system_clock += VDP_CYCLES_PER_LINE;
    }

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
    for (int i = 0; i < 256; ++i) {
      // TODO: update the palette
      // currentUpdate->palette[i] = (CRAM565[i] << 8) | (CRAM565[i] >> 8);

      //   // set the palette
      //   hal::set_palette(palette, PALETTE_SIZE);
    }
    // // rg_video_update_t *previousUpdate = &updates[currentUpdate == &updates[0]];

    // TODO: swap these out
    // rg_display_queue_update(currentUpdate, NULL);
    // // currentUpdate = previousUpdate;

    //   // render the frame
    //   hal::push_frame((uint8_t*)bitmap.data + frame_buffer_offset);
    //   // ping pong the frame buffer
    //   frame_buffer_index = !frame_buffer_index;
    //   bitmap.data = frame_buffer_index
    //     ? (uint8_t*)hal::get_frame_buffer1()
    //     : (uint8_t*)hal::get_frame_buffer0();

  }

  if (yfm_enabled || z80_enabled) {
    // TODO: swap these out
    // rg_audio_submit((void *)gwenesis_ym2612_buffer, AUDIO_BUFFER_LENGTH >> 1);

    // int16_t *audio_buffer = (int16_t*)hal::get_audio_buffer();
    // // push the audio buffer to the audio task
    // hal::play_audio((uint8_t*)audio_buffer, audio_buffer_len * 2);

  }

  // manage statistics
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration<float>(end-start).count();
  update_frame_time(elapsed);
}

void load_genesis(std::string_view save_path) {
  if (save_path.size()) {
    savestate_fp = fopen(save_path.data(), "rb");
    gwenesis_load_state();
    fclose(savestate_fp);
  }
  reset_genesis();
}

void save_genesis(std::string_view save_path) {
  // open the save path as a file descriptor
  savestate_fp = fopen(save_path.data(), "wb");
  gwenesis_save_state();
  fclose(savestate_fp);
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
