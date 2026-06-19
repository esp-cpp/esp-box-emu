#include "genesis.hpp"

#pragma GCC optimize("Ofast")

#include "genesis_shared_memory.hpp"
#include "genesis_dualcore.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <esp_heap_caps.h>

#if GENESIS_DUAL_CORE
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#endif

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

// Dual-core split: core 0 runs the 68000 + VDP, core 1 runs the sound unit
// (Z80 + YM2612 + SN76489). GENESIS_DUAL_CORE lives in genesis_dualcore.h. Set
// it to 0 for the single-core fallback (used as the muted path and a known-good
// reference). Only meaningful with GWENESIS_AUDIO_ACCURATE==0 (line-accurate
// audio), the configured build.

#if GENESIS_DUAL_CORE
// Lock-free SPSC queue of 68000-issued sound-chip register writes. Producer is
// core 0 (the 68k bus); consumer is the core-1 sound task. Entries are packed
// as (kind<<16)|(addr<<8)|value. In GWENESIS_AUDIO_ACCURATE==0 the write
// functions ignore their cycle target, so writes only need to be applied in
// FIFO order (per-line granularity), not at an exact sample.
static constexpr uint32_t SOUND_Q_SIZE = 1024; // power of two; 4 KB internal
static constexpr uint32_t SOUND_Q_MASK = SOUND_Q_SIZE - 1;
static uint32_t *sound_q = nullptr;
static std::atomic<uint32_t> sound_q_head{0};    // consumer index (core 1)
static std::atomic<uint32_t> sound_q_tail{0};    // producer index (core 0)
static std::atomic<uint32_t> sound_q_dropped{0};
static std::atomic<unsigned int> ym_status_snapshot{0};

// Per-line lockstep leash. Core 0 publishes the index of the last scanline whose
// 68000 step has completed; core 1 must not run a scanline's sound ahead of that,
// so 68k->sound register writes (queued during m68k_run) and the 68k<->Z80
// handoff stay aligned to the correct line instead of drifting up to a frame.
static std::atomic<int> core0_line{-1};

extern "C" void genesis_sound_queue_push(uint8_t kind, uint8_t addr, uint8_t value, int cycles) {
  (void)cycles; // ignored in line-accurate mode; kept for API/future use
  const uint32_t tail = sound_q_tail.load(std::memory_order_relaxed);
  const uint32_t head = sound_q_head.load(std::memory_order_acquire);
  if (tail - head >= SOUND_Q_SIZE) { // full: drop rather than block the 68k
    sound_q_dropped.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  sound_q[tail & SOUND_Q_MASK] =
    ((uint32_t)kind << 16) | ((uint32_t)addr << 8) | (uint32_t)value;
  sound_q_tail.store(tail + 1, std::memory_order_release);
}

extern "C" void genesis_sound_queue_drain(void) {
  uint32_t head = sound_q_head.load(std::memory_order_relaxed);
  const uint32_t tail = sound_q_tail.load(std::memory_order_acquire);
  for (; head != tail; ++head) {
    const uint32_t e = sound_q[head & SOUND_Q_MASK];
    const uint8_t kind = (uint8_t)(e >> 16);
    const uint8_t addr = (uint8_t)(e >> 8);
    const uint8_t value = (uint8_t)e;
    if (kind == GEN_SND_YM2612)
      YM2612Write(addr, value, 0);
    else
      gwenesis_SN76489_Write(value, 0);
  }
  sound_q_head.store(head, std::memory_order_release);
}

extern "C" void genesis_sound_queue_reset(void) {
  sound_q_head.store(0, std::memory_order_relaxed);
  sound_q_tail.store(0, std::memory_order_relaxed);
}

extern "C" uint32_t genesis_sound_queue_dropped(void) {
  return sound_q_dropped.load(std::memory_order_relaxed);
}

extern "C" void genesis_ym2612_status_publish(unsigned int status) {
  ym_status_snapshot.store(status, std::memory_order_relaxed);
}

extern "C" unsigned int genesis_ym2612_status_peek(void) {
  return ym_status_snapshot.load(std::memory_order_relaxed);
}
#endif // GENESIS_DUAL_CORE

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

#if !GENESIS_DUAL_CORE && GWENESIS_AUDIO_ACCURATE == 0
static IRAM_ATTR void run_genesis_frame_sound_on_no_frameskip(int screen_height, int lines_per_frame) {
  static constexpr int vdp_cycles_per_line = VDP_CYCLES_PER_LINE;
  int hint_counter = REG10_LINE_COUNTER;

  scan_line = 0;
  for (; scan_line < screen_height; ++scan_line) {
    system_clock += vdp_cycles_per_line;

    m68k_run(system_clock);
    z80_run(system_clock);
    gwenesis_SN76489_run(system_clock);
    ym2612_run(system_clock);
    gwenesis_vdp_render_line(scan_line);

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

    m68k_run(system_clock);
    z80_run(system_clock);
    gwenesis_SN76489_run(system_clock);
    ym2612_run(system_clock);
  }
}
#endif

#if GENESIS_DUAL_CORE && GWENESIS_AUDIO_ACCURATE == 0
// --- Dual-core frame execution (behind GENESIS_DUAL_CORE) -------------------
//
// The single-core loop above interleaves the 68k+VDP and the sound unit per
// scanline. For the dual-core split we run them as two independent per-frame
// loops that march through the SAME deterministic per-line clock
// (line*VDP_CYCLES_PER_LINE), so neither shares a clock write with the other:
//   - cpu_vdp_run_frame()    on core 0: 68000 + VDP render + 68k IRQs.
//   - sound_unit_run_frame() on core 1: Z80 + SN76489 + YM2612 + Z80 IRQs.
// The IRQ lines split cleanly by target CPU (m68k_set_irq/m68k_update_irq vs
// z80_irq_line). cpu_vdp owns the global scan_line (used by the VDP); the sound
// unit uses a private loop counter and clock so it never touches scan_line or
// system_clock.

// Core 0: 68000 + VDP. Owns system_clock and scan_line.
static IRAM_ATTR void cpu_vdp_run_frame(int screen_height, int lines_per_frame) {
  static constexpr int vdp_cycles_per_line = VDP_CYCLES_PER_LINE;
  int hint_counter = REG10_LINE_COUNTER;

  scan_line = 0;
  for (; scan_line < screen_height; ++scan_line) {
    system_clock += vdp_cycles_per_line;
    m68k_run(system_clock);
    // Release this line to the sound core: its 68k->sound writes are now queued.
    core0_line.store(scan_line, std::memory_order_release);
    gwenesis_vdp_render_line(scan_line);

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

  if (scan_line < lines_per_frame) {
    system_clock += vdp_cycles_per_line;
    m68k_run(system_clock);
    core0_line.store(scan_line, std::memory_order_release);

    if (--hint_counter < 0) {
      if (REG0_LINE_INTERRUPT != 0) {
        hint_pending = 1;
        if ((gwenesis_vdp_status & STATUS_VIRQPENDING) == 0)
          m68k_update_irq(4);
      }
      hint_counter = REG10_LINE_COUNTER;
    }

    ++scan_line;
  }

  for (; scan_line < lines_per_frame; ++scan_line) {
    system_clock += vdp_cycles_per_line;
    m68k_run(system_clock);
    core0_line.store(scan_line, std::memory_order_release);
  }
}

// Core 1: Z80 + SN76489 + YM2612. Uses a private clock that mirrors
// system_clock's per-line progression (both start at 0 each frame).
// Run one sound-unit scanline: apply any 68k-issued register writes (in FIFO
// order) before advancing the chips, then publish the YM2612 status for the
// 68k to read.
static inline IRAM_ATTR void sound_unit_scanline(int sclk) {
  genesis_sound_queue_drain();
  z80_run(sclk);
  gwenesis_SN76489_run(sclk);
  ym2612_run(sclk);
  genesis_ym2612_status_publish(YM2612Read(0));
}

// Leash: do not run this scanline's sound until core 0 has finished its 68000
// step for the same line. Cannot deadlock against a 68k BUSREQ — z80_halted is
// 1 between scanlines, so the 68k's halt-wait returns while we park here.
static inline IRAM_ATTR void sound_wait_for_line(int line) {
  // The sound unit is faster per scanline than the 68k+VDP, so it waits here a
  // good fraction of each frame. Yield instead of busy-spinning so the video
  // task (same core, same priority) gets that time — otherwise it is starved
  // and the display tears. A short spin first avoids a scheduler call when the
  // wait is only a few microseconds.
  while (core0_line.load(std::memory_order_acquire) < line) {
    for (int i = 0; i < 64; ++i) {
      if (core0_line.load(std::memory_order_acquire) >= line)
        return;
    }
    taskYIELD();
  }
}

static IRAM_ATTR void sound_unit_run_frame(int screen_height, int lines_per_frame) {
  static constexpr int vdp_cycles_per_line = VDP_CYCLES_PER_LINE;
  int sclk = 0;
  int line = 0;

  for (; line < screen_height; ++line) {
    sclk += vdp_cycles_per_line;
    sound_wait_for_line(line);
    sound_unit_scanline(sclk);
  }

  // Assert the Z80 V-int at the start of vblank and HOLD it across the whole
  // vblank period (all remaining scanlines), deasserting only at end of frame.
  // A one-scanline pulse is unreliable here: the Z80 frequently has interrupts
  // disabled during that single line, so V-int-driven sound drivers (e.g.
  // Sonic 2) miss it and stall. Holding it lets the Z80 take it as soon as it
  // re-enables interrupts; IAutoReset clears it on take so it fires only once.
  z80_irq_line(1);

  for (; line < lines_per_frame; ++line) {
    sclk += vdp_cycles_per_line;
    sound_wait_for_line(line);
    sound_unit_scanline(sclk);
  }

  z80_irq_line(0);

  // Apply any 68k writes that arrived after the last scanline drain, then mark
  // the Z80 quiescent so a 68k BUSREQ after this point sees it halted.
  genesis_sound_queue_drain();
  z80_mark_quiescent();
}

// --- Core-1 sound task + per-frame barrier ---------------------------------
// The sound task is pinned to core 1 at a priority above the video task so it
// runs promptly when a frame is dispatched (and so the BUSREQ handshake is
// acknowledged quickly), then blocks between frames, leaving core 1 to the
// video task. Coordination is two binary semaphores: core 0 gives "start",
// runs the 68k+VDP, then takes "done"; the sound task takes "start", runs the
// sound frame, then gives "done".
static TaskHandle_t sound_task_handle = nullptr;
static SemaphoreHandle_t sound_start_sem = nullptr;
static SemaphoreHandle_t sound_done_sem = nullptr;
static volatile int sound_frame_screen_height = 0;
static volatile int sound_frame_lines = 0;
static volatile bool sound_task_quit = false;

static void sound_task_fn(void *arg) {
  (void)arg;
  for (;;) {
    xSemaphoreTake(sound_start_sem, portMAX_DELAY);
    if (sound_task_quit)
      break;
    sound_unit_run_frame(sound_frame_screen_height, sound_frame_lines);
    xSemaphoreGive(sound_done_sem);
  }
  // Unblock any waiter and self-delete.
  xSemaphoreGive(sound_done_sem);
  sound_task_handle = nullptr;
  vTaskDelete(nullptr);
}

// Core 0: hand the frame to core 1.
static inline void sound_frame_start(int screen_height, int lines_per_frame) {
  sound_frame_screen_height = screen_height;
  sound_frame_lines = lines_per_frame;
  xSemaphoreGive(sound_start_sem);
}

// Core 0: barrier — wait for the sound task to finish the frame.
static inline void sound_frame_wait(void) {
  xSemaphoreTake(sound_done_sem, portMAX_DELAY);
}

static bool genesis_sound_task_create(void) {
  sound_task_quit = false;
  sound_start_sem = xSemaphoreCreateBinary();
  sound_done_sem = xSemaphoreCreateBinary();
  if (!sound_start_sem || !sound_done_sem)
    return false;
  // Priority 20 (same as the video task) and pinned to core 1, so that when the
  // sound unit yields during its leash wait the video task gets core 1 instead
  // of being starved. The BUSREQ handshake stays correct because z80_halted is
  // already 1 between scanlines (when the sound task would be preempted).
  BaseType_t ok = xTaskCreatePinnedToCore(sound_task_fn, "genesis_snd",
                                          4 * 1024, nullptr, 20,
                                          &sound_task_handle, 1);
  return ok == pdPASS;
}

static void genesis_sound_task_destroy(void) {
  if (sound_task_handle) {
    sound_task_quit = true;
    xSemaphoreGive(sound_start_sem); // wake the task so it observes quit
    // Wait for it to acknowledge (it gives done before deleting itself).
    if (sound_done_sem)
      xSemaphoreTake(sound_done_sem, pdMS_TO_TICKS(100));
    sound_task_handle = nullptr;
  }
  if (sound_start_sem) { vSemaphoreDelete(sound_start_sem); sound_start_sem = nullptr; }
  if (sound_done_sem) { vSemaphoreDelete(sound_done_sem); sound_done_sem = nullptr; }
}
#endif // GENESIS_DUAL_CORE

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

#if GENESIS_DUAL_CORE
  sound_q = (uint32_t*)allocate_hot_memory(SOUND_Q_SIZE * sizeof(uint32_t), "sound_q");
  genesis_sound_queue_reset();
  if (!sound_q) {
    fmt::print("Failed to allocate Genesis sound queue\n");
    deinit_genesis();
    return;
  }
  if (!genesis_sound_task_create()) {
    fmt::print("Failed to create Genesis sound task\n");
    deinit_genesis();
    return;
  }
#endif

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
#if GENESIS_DUAL_CORE
    // Dual-core: the audio clocks/indices (reset just above) and the write
    // queue are now consistent and the sound task is idle, so it is safe to
    // hand the frame to core 1, run the 68k+VDP here on core 0, then barrier.
    genesis_sound_queue_reset();
    core0_line.store(-1, std::memory_order_relaxed); // sem give below publishes it
    sound_frame_start(screen_height, lines_per_frame);
    cpu_vdp_run_frame(screen_height, lines_per_frame);
    sound_frame_wait();
#elif GWENESIS_AUDIO_ACCURATE == 0
    run_genesis_frame_sound_on_no_frameskip(screen_height, lines_per_frame);
#endif
  } else {
    while (scan_line < lines_per_frame) {
      system_clock += _vdp_cycles_per_line;

      m68k_run(system_clock);
      z80_run(system_clock);

      /* Audio */
      /*  GWENESIS_AUDIO_ACCURATE:
       *    =1 : cycle accurate mode. audio is refreshed when CPUs are performing a R/W access
       *    =0 : line  accurate mode. audio is refreshed every lines.
       */
      if (GWENESIS_AUDIO_ACCURATE == 0 && sound_enabled) {
        gwenesis_SN76489_run(system_clock);
        ym2612_run(system_clock);
      }

      /* Video */
      if (drawFrame && scan_line < screen_height)
        gwenesis_vdp_render_line(scan_line); /* render scan_line */

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
#if GENESIS_DUAL_CORE
  // Stop the core-1 sound task before tearing down the state it uses.
  genesis_sound_task_destroy();
#endif
  reset_genesis_runtime_state();
  BoxEmu::get().audio_sample_rate(48000);
  shared_mem_clear();
  free(M68K_RAM);
  free(lfo_pm_table);
#if GENESIS_DUAL_CORE
  free(sound_q);
  sound_q = nullptr;
  genesis_sound_queue_reset();
#endif
  M68K_RAM = nullptr;
  lfo_pm_table = nullptr;
  palette = nullptr;
  gwenesis_sn76489_buffer = nullptr;
  gwenesis_ym2612_buffer = nullptr;
  frame_buffer = nullptr;
}
