#include "genesis.hpp"

#pragma GCC optimize("Ofast")

#include "genesis_shared_memory.hpp"

#include <algorithm>
#include <cstring>
#include <esp_heap_caps.h>

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

#include "box-emu.hpp"
#include "statistics.hpp"

static constexpr int AUDIO_BUFFER_LENGTH = std::max(GWENESIS_AUDIO_BUFFER_LENGTH_NTSC, GWENESIS_AUDIO_BUFFER_LENGTH_PAL);
static constexpr int AUDIO_OUTPUT_CHANNELS = 2;
static constexpr int GENESIS_AUDIO_SAMPLE_STEP = 2;

static constexpr size_t GENESIS_SCREEN_WIDTH = 320;
static constexpr size_t GENESIS_VISIBLE_HEIGHT = 224;

static constexpr size_t PALETTE_SIZE = 256;
static uint16_t *palette = nullptr;
static bool genesis_initialized = false;

static int frame_counter = 0;
static uint16_t muteFrameCount = 0;
static int frame_buffer_index = 0;
static uint8_t *frame_buffer = nullptr;
static GamepadState previous_gamepad_state = {};

/// BEGIN GWENESIS EMULATOR

extern int zclk;
static int system_clock;
int scan_line;

int16_t *gwenesis_sn76489_buffer = nullptr;
int sn76489_index;
int sn76489_clock;
int16_t *gwenesis_ym2612_buffer = nullptr;
int ym2612_index;
int ym2612_clock;

static constexpr int full_frameskip = 1;
static constexpr int muted_frameskip = 1;
static int frameskip = full_frameskip;

// Per-subsystem profiling. Set to 0 to remove all timing overhead once we've
// identified the bottleneck. Prints a per-frame breakdown every PROFILE_PERIOD
// frames.
#define GENESIS_PROFILE 0
#if GENESIS_PROFILE
static constexpr int GENESIS_PROFILE_PERIOD = 300;
static uint64_t prof_m68k = 0, prof_z80 = 0, prof_sn = 0, prof_ym = 0, prof_vdp = 0;
static int prof_frames = 0;
#endif

static FILE *savestate_fp = NULL;
static int savestate_errors = 0;

// GW_TARGET=1 build only indexes 128*8*16 entries and stores values in 0..255,
// so int16_t is sufficient. This is 32768 bytes vs the old 131072 bytes, which
// makes it far more likely to land in internal RAM and halves the per-sample
// LFO load width. (Previously declared int32_t[128*8*32] = 131072 bytes.)
int16_t *lfo_pm_table = nullptr; // 128*8*16*sizeof(int16_t) = 32768 bytes

static inline int16_t clamp_audio_sample(int sample) {
  return (int16_t)std::clamp(sample, -32768, 32767);
}

extern unsigned char *gwenesis_vdp_regs; // [0x20];
extern unsigned char *VRAM; // [VRAM_MAX_SIZE];
extern unsigned short *CRAM; // [CRAM_MAX_SIZE];
extern unsigned char *SAT_CACHE; // [SAT_CACHE_MAX_SIZE];
extern unsigned short *fifo; // [FIFO_SIZE];
extern unsigned int gwenesis_vdp_status;
extern unsigned short *CRAM565; // [256];
extern unsigned short *VSRAM; // [VSRAM_MAX_SIZE];
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

static void reset_genesis_runtime_state() {
  system_clock = 0;
  scan_line = 0;
  frame_counter = 0;
  muteFrameCount = 0;
  frame_buffer_index = 0;
  previous_gamepad_state = {};

  zclk = 0;
  ym2612_index = 0;
  ym2612_clock = 0;
  sn76489_index = 0;
  sn76489_clock = 0;

  if (gwenesis_sn76489_buffer) {
    std::memset(gwenesis_sn76489_buffer, 0, AUDIO_BUFFER_LENGTH * sizeof(int16_t));
  }
  if (gwenesis_ym2612_buffer) {
    std::memset(gwenesis_ym2612_buffer, 0, AUDIO_BUFFER_LENGTH * sizeof(int16_t));
  }

  for (int i = 0; i < 8; i++) {
    gwenesis_io_pad_release_button(0, i);
  }
}

static void *allocate_hot_memory(size_t size, const char *name = "hot") {
  void *ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const bool internal = (ptr != nullptr);
  if (!ptr) {
    ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  }
  fmt::print("[genesis mem] {}: {} bytes -> {}\n", name, size, internal ? "INTERNAL" : "PSRAM");
  return ptr;
}

static void *allocate_shared_hot_memory(size_t size, shared_mem_region_t region = SHARED_MEM_DEFAULT) {
  shared_mem_request_t request = {
    .size = size,
    .region = region,
    .storage = SHARED_MEM_INTERNAL,
  };
  return shared_mem_allocate(&request);
}

#if GWENESIS_AUDIO_ACCURATE == 0
static IRAM_ATTR void run_genesis_frame_sound_on_no_frameskip(int screen_height, int lines_per_frame) {
  static constexpr int vdp_cycles_per_line = VDP_CYCLES_PER_LINE;
  int hint_counter = REG10_LINE_COUNTER;

  scan_line = 0;
  for (; scan_line < screen_height; ++scan_line) {
    system_clock += vdp_cycles_per_line;

#if GENESIS_PROFILE
    uint64_t _pt0 = esp_timer_get_time();
#endif
    m68k_run(system_clock);
#if GENESIS_PROFILE
    uint64_t _pt1 = esp_timer_get_time(); prof_m68k += _pt1 - _pt0;
#endif
    z80_run(system_clock);
#if GENESIS_PROFILE
    uint64_t _pt2 = esp_timer_get_time(); prof_z80 += _pt2 - _pt1;
#endif
    gwenesis_SN76489_run(system_clock);
#if GENESIS_PROFILE
    uint64_t _pta = esp_timer_get_time(); prof_sn += _pta - _pt2;
#endif
    ym2612_run(system_clock);
#if GENESIS_PROFILE
    uint64_t _ptb = esp_timer_get_time(); prof_ym += _ptb - _pta;
#endif
    gwenesis_vdp_render_line(scan_line);
#if GENESIS_PROFILE
    uint64_t _pt4 = esp_timer_get_time(); prof_vdp += _pt4 - _ptb;
#endif

    if (--hint_counter < 0) {
      if (REG0_LINE_INTERRUPT != 0) {
        hint_pending = 1;
        if ((gwenesis_vdp_status & STATUS_VIRQPENDING) == 0)
          m68k_update_irq(4);
      }
      hint_counter = REG10_LINE_COUNTER;
    }
  }

  if (REG1_VBLANK_INTERRUPT != 0) {
    gwenesis_vdp_status |= STATUS_VIRQPENDING;
    m68k_set_irq(6);
  }
  z80_irq_line(1);

  if (scan_line < lines_per_frame) {
    system_clock += vdp_cycles_per_line;

    m68k_run(system_clock);
    z80_run(system_clock);
    gwenesis_SN76489_run(system_clock);
    ym2612_run(system_clock);

    if (--hint_counter < 0) {
      if (REG0_LINE_INTERRUPT != 0) {
        hint_pending = 1;
        if ((gwenesis_vdp_status & STATUS_VIRQPENDING) == 0)
          m68k_update_irq(4);
      }
      hint_counter = REG10_LINE_COUNTER;
    }

    ++scan_line;
    z80_irq_line(0);
  }

  for (; scan_line < lines_per_frame; ++scan_line) {
    system_clock += vdp_cycles_per_line;

#if GENESIS_PROFILE
    uint64_t _qt0 = esp_timer_get_time();
#endif
    m68k_run(system_clock);
#if GENESIS_PROFILE
    uint64_t _qt1 = esp_timer_get_time(); prof_m68k += _qt1 - _qt0;
#endif
    z80_run(system_clock);
#if GENESIS_PROFILE
    uint64_t _qt2 = esp_timer_get_time(); prof_z80 += _qt2 - _qt1;
#endif
    gwenesis_SN76489_run(system_clock);
#if GENESIS_PROFILE
    uint64_t _qta = esp_timer_get_time(); prof_sn += _qta - _qt2;
#endif
    ym2612_run(system_clock);
#if GENESIS_PROFILE
    uint64_t _qtb = esp_timer_get_time(); prof_ym += _qtb - _qta;
#endif
  }
}
#endif

static void init(uint8_t *romdata, size_t rom_data_size) {
  genesis_initialized = false;

  // M68K work RAM is the hottest, most random-access structure in the emulator:
  // it is touched by nearly every 68000 instruction, so PSRAM latency here
  // dominates the frame time. Internal RAM is too fragmented to hold both this
  // (64 KB) and VRAM (64 KB), so allocate M68K_RAM FIRST to win the scarce large
  // internal block; VRAM is downgraded to prefer-internal (PSRAM fallback) in
  // genesis_init_shared_memory() and is accessed more sequentially during render.
  M68K_RAM = (uint8_t*)allocate_hot_memory(MAX_RAM_SIZE, "M68K_RAM");

  genesis_init_shared_memory();

  // local shared memory (used in this file):
  palette = (uint16_t*)allocate_shared_hot_memory(sizeof(uint16_t) * PALETTE_SIZE);
  gwenesis_sn76489_buffer = (int16_t*)allocate_shared_hot_memory(AUDIO_BUFFER_LENGTH * sizeof(int16_t), SHARED_MEM_CACHE_LINE);
  gwenesis_ym2612_buffer = (int16_t*)allocate_shared_hot_memory(AUDIO_BUFFER_LENGTH * sizeof(int16_t), SHARED_MEM_CACHE_LINE);

  lfo_pm_table = (int16_t*)allocate_hot_memory(128*8*16 * sizeof(int16_t), "lfo_pm_table");

  if (!palette || !gwenesis_sn76489_buffer || !gwenesis_ym2612_buffer || !M68K_RAM || !lfo_pm_table ||
      !VRAM || !ZRAM || !ym2612 || !OPNREGS || !sin_tab || !render_buffer || !sprite_buffer ||
      !CRAM || !SAT_CACHE || !gwenesis_vdp_regs || !fifo || !CRAM565 || !VSRAM || !tl_tab) {
    fmt::print("Failed to allocate Genesis runtime memory\n");
    deinit_genesis();
    return;
  }

  YM2612SetSampleStep(GENESIS_AUDIO_SAMPLE_STEP);
  load_cartridge(romdata, rom_data_size);
  power_on();
  reset_genesis();

  if (REG1_PAL) {
    gwenesis_SN76489_Init(3546895,
      (GWENESIS_AUDIO_BUFFER_LENGTH_PAL / GENESIS_AUDIO_SAMPLE_STEP) * GWENESIS_REFRESH_RATE_PAL,
      AUDIO_FREQ_DIVISOR * GENESIS_AUDIO_SAMPLE_STEP);
  } else {
    gwenesis_SN76489_Init(3579545,
      (GWENESIS_AUDIO_BUFFER_LENGTH_NTSC / GENESIS_AUDIO_SAMPLE_STEP) * GWENESIS_REFRESH_RATE_NTSC,
      AUDIO_FREQ_DIVISOR * GENESIS_AUDIO_SAMPLE_STEP);
  }

  reset_genesis_runtime_state();

  BoxEmu::get().audio_sample_rate(REG1_PAL ? GWENESIS_AUDIO_FREQ_PAL/2 : GWENESIS_AUDIO_FREQ_NTSC/2);
  BoxEmu::get().palette(palette, PALETTE_SIZE);

  frame_buffer = frame_buffer_index
    ? BoxEmu::get().frame_buffer1()
    : BoxEmu::get().frame_buffer0();
  gwenesis_vdp_set_buffer(frame_buffer);

  fmt::print("Num bytes allocated: {}\n", shared_num_bytes_allocated());

  reset_frame_time();
  genesis_initialized = true;
}

void init_genesis(uint8_t *romdata, size_t rom_data_size) {
  BoxEmu::get().native_size(GENESIS_SCREEN_WIDTH, GENESIS_VISIBLE_HEIGHT, GENESIS_SCREEN_WIDTH);
  init(romdata, rom_data_size);
}

void IRAM_ATTR run_genesis_rom() {
  if (!genesis_initialized) {
    return;
  }
  auto start = esp_timer_get_time();
  // handle input here (see system.h and use input.pad and input.system)
  auto state = BoxEmu::get().gamepad_state();

  bool sound_enabled = !BoxEmu::get().is_muted();

  frameskip = sound_enabled ? full_frameskip : muted_frameskip;
  const bool fast_sound_path =
#if GWENESIS_AUDIO_ACCURATE == 0
    sound_enabled && full_frameskip == 1;
#else
    false;
#endif

  if (previous_gamepad_state != state) {
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

  previous_gamepad_state = state;

  const int current_frame = frame_counter++;
  bool drawFrame = fast_sound_path ? true : ((current_frame % frameskip) == 0);

  int lines_per_frame = REG1_PAL ? LINES_PER_FRAME_PAL : LINES_PER_FRAME_NTSC;
  int hint_counter = gwenesis_vdp_regs[10];

  screen_width = REG12_MODE_H40 ? 320 : 256;
  screen_height = REG1_PAL ? 240 : 224;

  gwenesis_vdp_render_config();

  /* Reset the difference clocks and audio index */
  system_clock = 0;
  zclk = sound_enabled ? 0 : 0x1000000;

  ym2612_clock = sound_enabled ? 0 : 0x1000000;
  ym2612_index = 0;

  sn76489_clock = sound_enabled ? 0 : 0x1000000;
  sn76489_index = 0;

  scan_line = 0;

  static constexpr int _vdp_cycles_per_line = VDP_CYCLES_PER_LINE;

  if (fast_sound_path) {
#if GWENESIS_AUDIO_ACCURATE == 0
    run_genesis_frame_sound_on_no_frameskip(screen_height, lines_per_frame);
#endif
  } else {
    while (scan_line < lines_per_frame) {
      system_clock += _vdp_cycles_per_line;

#if GENESIS_PROFILE
      uint64_t _pt0 = esp_timer_get_time();
#endif
      m68k_run(system_clock);
#if GENESIS_PROFILE
      uint64_t _pt1 = esp_timer_get_time(); prof_m68k += _pt1 - _pt0;
#endif
      z80_run(system_clock);
#if GENESIS_PROFILE
      uint64_t _pt2 = esp_timer_get_time(); prof_z80 += _pt2 - _pt1;
#endif

      /* Audio */
      /*  GWENESIS_AUDIO_ACCURATE:
       *    =1 : cycle accurate mode. audio is refreshed when CPUs are performing a R/W access
       *    =0 : line  accurate mode. audio is refreshed every lines.
       */
      if (GWENESIS_AUDIO_ACCURATE == 0 && sound_enabled) {
        gwenesis_SN76489_run(system_clock);
#if GENESIS_PROFILE
        uint64_t _pta = esp_timer_get_time(); prof_sn += _pta - _pt2;
#endif
        ym2612_run(system_clock);
#if GENESIS_PROFILE
        uint64_t _ptb = esp_timer_get_time(); prof_ym += _ptb - _pta;
#endif
      }
#if GENESIS_PROFILE
      uint64_t _pt3 = esp_timer_get_time();
#endif

      /* Video */
      if (drawFrame && scan_line < screen_height)
        gwenesis_vdp_render_line(scan_line); /* render scan_line */
#if GENESIS_PROFILE
      uint64_t _pt4 = esp_timer_get_time(); prof_vdp += _pt4 - _pt3;
#endif

      // On these lines, the line counter interrupt is reloaded
      if ((scan_line == 0) || (scan_line > screen_height)) {
        hint_counter = REG10_LINE_COUNTER;
      }

      // interrupt line counter
      if (--hint_counter < 0) {
        if ((REG0_LINE_INTERRUPT != 0) && (scan_line <= screen_height)) {
          hint_pending = 1;
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
  }

  /* Audio
   * synchronize YM2612 and SN76489 to system_clock
   * it completes the missing audio sample for accurate audio mode
   */
  if (GWENESIS_AUDIO_ACCURATE == 1 && sound_enabled) {
    gwenesis_SN76489_run(system_clock);
    ym2612_run(system_clock);
  }

  // reset m68k cycles to the begin of next frame cycle
  m68k->cycles -= system_clock;

  if (drawFrame) {
    // copy the palette
    memcpy(palette, CRAM565, PALETTE_SIZE * sizeof(uint16_t));
    // push the frame buffer to the display task
    BoxEmu::get().push_frame(frame_buffer);
    // ping pong the frame buffer
    frame_buffer_index = !frame_buffer_index;
    frame_buffer = frame_buffer_index
      ? BoxEmu::get().frame_buffer1()
      : BoxEmu::get().frame_buffer0();
    gwenesis_vdp_set_buffer(frame_buffer);
  }

  if (sound_enabled) {
    const int max_audio_frames = AUDIO_BUFFER_LENGTH / AUDIO_OUTPUT_CHANNELS;
    int audio_len = std::min(max_audio_frames, std::max(sn76489_index, ym2612_index));
    const int16_t* sn76489_buffer = gwenesis_sn76489_buffer;
    int16_t* ym2612_buffer = gwenesis_ym2612_buffer;
    const int shared_audio_len = std::min(sn76489_index, ym2612_index);
    int i = audio_len - 1;

    if (ym2612_index > sn76489_index) {
      for (; i >= shared_audio_len; i--) {
        const int16_t sample = ym2612_buffer[i];
        ym2612_buffer[i * AUDIO_OUTPUT_CHANNELS + 0] = sample;
        ym2612_buffer[i * AUDIO_OUTPUT_CHANNELS + 1] = sample;
      }
    } else {
      for (; i >= shared_audio_len; i--) {
        const int16_t sample = sn76489_buffer[i];
        ym2612_buffer[i * AUDIO_OUTPUT_CHANNELS + 0] = sample;
        ym2612_buffer[i * AUDIO_OUTPUT_CHANNELS + 1] = sample;
      }
    }

    for (; i >= 3; i -= 4) {
      const int16_t sample3 = clamp_audio_sample((int)ym2612_buffer[i - 0] + (int)sn76489_buffer[i - 0]);
      const int16_t sample2 = clamp_audio_sample((int)ym2612_buffer[i - 1] + (int)sn76489_buffer[i - 1]);
      const int16_t sample1 = clamp_audio_sample((int)ym2612_buffer[i - 2] + (int)sn76489_buffer[i - 2]);
      const int16_t sample0 = clamp_audio_sample((int)ym2612_buffer[i - 3] + (int)sn76489_buffer[i - 3]);

      ym2612_buffer[(i - 0) * AUDIO_OUTPUT_CHANNELS + 0] = sample3;
      ym2612_buffer[(i - 0) * AUDIO_OUTPUT_CHANNELS + 1] = sample3;
      ym2612_buffer[(i - 1) * AUDIO_OUTPUT_CHANNELS + 0] = sample2;
      ym2612_buffer[(i - 1) * AUDIO_OUTPUT_CHANNELS + 1] = sample2;
      ym2612_buffer[(i - 2) * AUDIO_OUTPUT_CHANNELS + 0] = sample1;
      ym2612_buffer[(i - 2) * AUDIO_OUTPUT_CHANNELS + 1] = sample1;
      ym2612_buffer[(i - 3) * AUDIO_OUTPUT_CHANNELS + 0] = sample0;
      ym2612_buffer[(i - 3) * AUDIO_OUTPUT_CHANNELS + 1] = sample0;
    }

    for (; i >= 0; i--) {
      const int16_t sample = clamp_audio_sample((int)ym2612_buffer[i] + (int)sn76489_buffer[i]);
      ym2612_buffer[i * AUDIO_OUTPUT_CHANNELS + 0] = sample;
      ym2612_buffer[i * AUDIO_OUTPUT_CHANNELS + 1] = sample;
    }
    BoxEmu::get().play_audio((uint8_t*)ym2612_buffer, audio_len * AUDIO_OUTPUT_CHANNELS * sizeof(int16_t));
  }

  // manage statistics
  auto end = esp_timer_get_time();
  uint64_t elapsed = end - start;
  update_frame_time(elapsed);

#if GENESIS_PROFILE
  prof_frames++;
  if (prof_frames >= GENESIS_PROFILE_PERIOD) {
    const double f = prof_frames;
    fmt::print("[genesis prof] per-frame us  m68k:{:.0f}  z80:{:.0f}  sn76489:{:.0f}  ym2612:{:.0f}  vdp:{:.0f}  (drawFrame counts render)\n",
               prof_m68k / f, prof_z80 / f, prof_sn / f, prof_ym / f, prof_vdp / f);
    prof_m68k = prof_z80 = prof_sn = prof_ym = prof_vdp = 0;
    prof_frames = 0;
  }
#endif
  static constexpr uint64_t max_frame_time = 1000000 / 60;
  if (elapsed < max_frame_time) {
    auto sleep_time = (max_frame_time - elapsed) / 1e3;
    std::this_thread::sleep_for(sleep_time * std::chrono::milliseconds(1));
  } else {
    vTaskDelay(1);
  }
}

void load_genesis(std::string_view save_path) {
  if (!genesis_initialized) {
    return;
  }
  if (save_path.size()) {
    savestate_fp = fopen(save_path.data(), "rb");
    gwenesis_load_state();
    fclose(savestate_fp);
  }
}

void save_genesis(std::string_view save_path) {
  if (!genesis_initialized) {
    return;
  }
  // open the save path as a file descriptor
  savestate_fp = fopen(save_path.data(), "wb");
  gwenesis_save_state();
  fclose(savestate_fp);
}

std::span<uint8_t> get_genesis_video_buffer() {
  if (!genesis_initialized || !frame_buffer || !palette) {
    return {};
  }
  static constexpr int height = GENESIS_VISIBLE_HEIGHT;
  static constexpr int width = GENESIS_SCREEN_WIDTH;

  auto *span_buffer = !frame_buffer_index
    ? BoxEmu::get().frame_buffer1()
    : BoxEmu::get().frame_buffer0();

  // make a span from the _other_ frame buffer so we can reuse memory
  // this is a bit of a hack, but it works
  std::span<uint8_t> frame(span_buffer, width * height * 2);

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
  genesis_initialized = false;
  reset_genesis_runtime_state();
  BoxEmu::get().audio_sample_rate(48000);
  shared_mem_clear();
  free(M68K_RAM);
  free(lfo_pm_table);
  M68K_RAM = nullptr;
  lfo_pm_table = nullptr;
  palette = nullptr;
  gwenesis_sn76489_buffer = nullptr;
  gwenesis_ym2612_buffer = nullptr;
  frame_buffer = nullptr;
}
