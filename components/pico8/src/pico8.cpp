// PICO-8 glue for esp-box-emu, hosting femto8.
//
// femto8's p8_run() owns the cart's main loop (Lua _update/_draw, flip, frame
// pacing) and returns only when the cart ends, so the cart runs in its own
// FreeRTOS task and run_pico8_rom() just paces the emulator loop. The platform
// seams femto8 exposes (p8_espbox.h) are small: a frame of palette indices to
// present, the buttons, and a pump called from the Lua instruction hook —
// which is where the emulator menu pauses the cart and where a quit from the
// menu unwinds it (p8_quit() longjmps out of the interpreter exactly the way
// femto8's own escape key does).
//
// femto8 was written to run once per process; its statics (the Lua state,
// setjmp buffers, cart flags) are reset between launches through the linker
// SURROUND symbols in linker.lf, the same way the MGS core does it.
#include "pico8.hpp"

#include "box-emu.hpp"
#include "statistics.hpp"
#include "platform/p8_espbox.h"
#include "platform/p8_lua_alloc.h"
#include "platform/p8_state.h"

#define PICO8_PROF_MAX_REPORT 8
extern "C" {
#include "p8_emu.h"
#include "p8_input.h"
#include "p8_audio.h"
#include "p8_lua.h"
void render_sounds(int16_t* buffer, int total_samples); // p8_audio.c
#ifdef PICO8_PROFILE
void pico8_lua_bench(void); // p8_lua.c
extern uint32_t pico8_prof_gc_cycles, pico8_prof_gc_steps, pico8_prof_hooks;
#endif
}

#include <esp_debug_helpers.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

extern "C" {
// linker.lf SURROUND symbols for libpico8.a's statics
// (declared as single chars: only their addresses mean anything, and the
// section bounds are computed as integers below)
extern char _pico8_bss_start, _pico8_bss_end;
extern char _pico8_common_start, _pico8_common_end;
extern char _pico8_data_start, _pico8_data_end;
// femto8's 32-entry RGB565 palette (p8_emu.c)
extern uint16_t m_colors[32];
}

namespace {
  constexpr int P8_W = 128;
  constexpr int P8_H = 128;
  constexpr size_t FRAME_BYTES = P8_W * P8_H; // 8-bit indices
  // the cart task: Lua recursion and lodepng want a roomy stack; internal RAM
  // because the cart loader reads from the card
  constexpr size_t CART_TASK_STACK = 24 * 1024;
  constexpr UBaseType_t CART_TASK_PRIO = 5;
  constexpr UBaseType_t EMU_TASK_PRIO_WHILE_RUNNING = 6;
  // audio: femto8 renders mono S16 at SAMPLE_RATE; the box wants stereo frames
  constexpr int AUDIO_CHUNK = 512; // frames per write (~11.6ms at 44.1kHz)

  std::string s_cartPath;
  bool s_initialized = false;
  std::atomic<bool> s_paused{false};
  std::atomic<bool> s_quit{false};        // the cart is over (or never started)
  std::atomic<bool> s_stopRequested{false}; // the emulator wants the cart gone
  std::atomic<bool> s_cartTaskDone{false};
  // a save/load requested from the menu, done by the cart task between frames
  enum class StateOp { None, Save, Load };
  std::atomic<StateOp> s_stateOp{StateOp::None};
  std::atomic<bool> s_stateOpDone{false};
  std::atomic<int> s_stateOpResult{0};
  std::string s_statePath;
  uint8_t* s_dataSnapshot = nullptr;

  TaskHandle_t s_cartTask = nullptr;
  StackType_t* s_cartStack = nullptr;
  StaticTask_t* s_cartTcb = nullptr;
  TaskHandle_t s_audioTask = nullptr;
  std::atomic<bool> s_audioRun{false};
  std::atomic<bool> s_audioReady{false}; // femto8's memory exists (between p8_init and p8_shutdown)
  std::atomic<bool> s_audioTaskDone{false};
  int16_t* s_audioMono = nullptr;
  int16_t* s_audioStereo = nullptr;
  UBaseType_t s_emuTaskPrio = 1;

  uint8_t* s_frames[2] = {nullptr, nullptr}; // 128x128 indices, in frame_buffer0
  int s_frameIndex = 0;
  const uint8_t* s_lastFrame = nullptr;
  unsigned s_framesPresented = 0;
  unsigned s_lastPresented = 0;
  int64_t s_lastFrameUs = 0, s_lastReportUs = 0;
  unsigned s_framesAtReport = 0;


  // A section delimited by two linker symbols.
  struct Section {
    uint8_t* begin;
    size_t size;
  };
  Section section(char& start, char& end) {
    const uintptr_t b = reinterpret_cast<uintptr_t>(&start);
    const uintptr_t e = reinterpret_cast<uintptr_t>(&end);
    return Section{reinterpret_cast<uint8_t*>(b), static_cast<size_t>(e - b)};
  }

  void logMemory(const char* when) {
    fmt::print("[PICO8] {}: free internal {} B (largest {} B), free PSRAM {} B (largest {} B)\n", when,
               heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
               heap_caps_get_free_size(MALLOC_CAP_SPIRAM), heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
  }

  // Put every static of libpico8.a back to its as-linked value. Only valid
  // while none of femto8's tasks exist.
  void resetStatics() {
    const Section bss = section(_pico8_bss_start, _pico8_bss_end);
    const Section common = section(_pico8_common_start, _pico8_common_end);
    const Section data = section(_pico8_data_start, _pico8_data_end);
    if (!s_dataSnapshot) {
      s_dataSnapshot = static_cast<uint8_t*>(heap_caps_malloc(data.size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
      if (s_dataSnapshot) {
        memcpy(s_dataSnapshot, data.begin, data.size);
      }
      fmt::print("[PICO8] statics: bss {} B, common {} B, data {} B (snapshot {})\n", bss.size, common.size, data.size,
                 s_dataSnapshot ? "ok" : "FAILED");
      return;
    }
    memset(bss.begin, 0, bss.size);
    memset(common.begin, 0, common.size);
    memcpy(data.begin, s_dataSnapshot, data.size);
  }

  void cartTask(void*) {
    fmt::print("[PICO8] cart task on core {}\n", xPortGetCoreID());
    if (p8_init() != 0) {
      fmt::print("[PICO8] p8_init failed\n");
    } else {
      s_audioReady = true;
      if (p8_load(s_cartPath.c_str(), nullptr, nullptr, nullptr) != 0) {
        fmt::print("[PICO8] could not load '{}'\n", s_cartPath);
      } else {
        const int ret = p8_run();
        if (ret != 0) {
          fmt::print("[PICO8] cart ended with an error ({})\n", ret);
          lua_print_error();
        } else {
          fmt::print("[PICO8] cart ended\n");
        }
      }
    }
    // the audio task must not touch femto8's memory once it is freed
    s_audioReady = false;
    vTaskDelay(pdMS_TO_TICKS(20));
    p8_shutdown();
    s_quit = true;
    s_cartTaskDone = true;
    vTaskSuspend(nullptr);
    for (;;) {
      vTaskDelay(portMAX_DELAY);
    }
  }

  void audioTask(void*) {
    constexpr size_t CHUNK_BYTES = AUDIO_CHUNK * 2 * sizeof(int16_t);
    size_t queued = CHUNK_BYTES; // nothing pending yet
    while (s_audioRun) {
      if (s_paused || !s_audioReady) {
        vTaskDelay(pdMS_TO_TICKS(20));
        continue;
      }
      if (queued >= CHUNK_BYTES) {
        // what femto8's SDL callback does, fed by hand
        render_sounds(s_audioMono, AUDIO_CHUNK);
        for (int i = 0; i < AUDIO_CHUNK; i++) {
          s_audioStereo[2 * i] = s_audioMono[i];
          s_audioStereo[2 * i + 1] = s_audioMono[i];
        }
        queued = 0;
      }
      // play_audio queues what fits and returns the count; the I2S stream
      // buffer drains at the sample rate, so wait a little when it is full
      // (the BSP's play_audio returns the count; BoxEmu's wrapper discards it)
      queued += BoxEmu::Bsp::get().play_audio(reinterpret_cast<const uint8_t*>(s_audioStereo) + queued, CHUNK_BYTES - queued);
      if (queued < CHUNK_BYTES) {
        vTaskDelay(pdMS_TO_TICKS(3));
      }
    }
    s_audioTaskDone = true;
    vTaskSuspend(nullptr);
    for (;;) {
      vTaskDelay(portMAX_DELAY);
    }
  }

  // Wait for a task to park itself after being asked to stop.
  bool waitFor(std::atomic<bool>& flag, int maxMs) {
    for (int waited = 0; waited < maxMs && !flag; waited += 5) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
    return flag;
  }
}

// femto8's main.c (not built) defines this for the version dialog
extern "C" const char* femto8_version = "1.0.00";

// ---- femto8 platform hooks (p8_espbox.h) -----------------------------------

extern "C" uint8_t* p8_espbox_frame_begin(void) {
  return s_frames[s_frameIndex];
}

extern "C" void p8_espbox_frame_end(void) {
  const uint8_t* frame = s_frames[s_frameIndex];
  s_frameIndex ^= 1;
  s_lastFrame = frame;
  s_framesPresented++;
  if (!s_paused) {
    BoxEmu::get().push_frame(frame);
  }
}

extern "C" uint16_t p8_espbox_buttons(void) {
  // PICO-8: left right up down O X; pause opens PICO-8's own menu
  const GamepadState state = BoxEmu::get().gamepad_state();
  uint16_t mask = 0;
  if (state.left)  mask |= BUTTON_MASK_LEFT;
  if (state.right) mask |= BUTTON_MASK_RIGHT;
  if (state.up)    mask |= BUTTON_MASK_UP;
  if (state.down)  mask |= BUTTON_MASK_DOWN;
  if (state.a || state.y) mask |= BUTTON_MASK_ACTION1; // O
  if (state.b || state.x) mask |= BUTTON_MASK_ACTION2; // X
  if (state.start && !state.select) mask |= BUTTON_MASK_PAUSE;
  return mask;
}

extern "C" void p8_espbox_pump(void) {
  // the emulator menu holds the cart here (the Lua VM is at a safe point);
  // a pending save/load lets it run on to the end of the frame first
  while (s_paused && !s_stopRequested && s_stateOp == StateOp::None) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (s_stopRequested) {
    p8_quit(); // longjmps out of the interpreter; p8_run() returns
  }
}

extern "C" void p8_espbox_frame_boundary(void) {
  const StateOp op = s_stateOp;
  if (op == StateOp::None) {
    return;
  }
  s_stateOpResult = (op == StateOp::Save) ? p8_state_save(s_statePath.c_str()) : p8_state_load(s_statePath.c_str());
  s_stateOp = StateOp::None;
  s_stateOpDone = true;
}

// Ask the cart task to save/load at its next frame boundary and wait for it.
// Called from the emulator task while the menu has the cart paused.
static bool runStateOp(StateOp op, std::string_view path) {
  if (!s_initialized || s_quit || path.empty()) {
    return false;
  }
  s_statePath = std::string(path);
  s_stateOpDone = false;
  s_stateOp = op;
  // a frame plus the file I/O; a cart stuck in its own pause menu never
  // reaches the boundary, so give up eventually
  if (!waitFor(s_stateOpDone, 15000)) {
    fmt::print("[PICO8] {} state: the cart did not reach a frame boundary\n", op == StateOp::Save ? "save" : "load");
    s_stateOp = StateOp::None;
    return false;
  }
  return s_stateOpResult == 0;
}

// ---- emulator API -----------------------------------------------------------

void init_pico8(const std::string& rom_filename, uint8_t* romdata, size_t rom_data_size) {
  (void)romdata; (void)rom_data_size;
  if (s_initialized) {
    return;
  }
  s_cartPath = rom_filename;
  s_quit = false;
  s_paused = false;
  s_stopRequested = false;
  s_cartTaskDone = false;
  s_audioTaskDone = false;
  s_lastFrame = nullptr;
  s_frameIndex = 0;
  s_framesPresented = s_lastPresented = s_framesAtReport = 0;
  s_lastFrameUs = s_lastReportUs = esp_timer_get_time();
  logMemory("before init");

  resetStatics();

  auto& box = BoxEmu::get();
  // two index frames in frame_buffer0 (it is far bigger than 2 x 16KB)
  s_frames[0] = box.frame_buffer0();
  s_frames[1] = box.frame_buffer0() + FRAME_BYTES;
  memset(s_frames[0], 0, 2 * FRAME_BYTES);
  box.native_size(P8_W, P8_H);
  box.palette(m_colors, 32);

  s_audioMono = (int16_t*)heap_caps_malloc(AUDIO_CHUNK * sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  s_audioStereo = (int16_t*)heap_caps_malloc(AUDIO_CHUNK * 2 * sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  s_cartStack = (StackType_t*)heap_caps_malloc(CART_TASK_STACK, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  s_cartTcb = (StaticTask_t*)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!s_audioMono || !s_audioStereo || !s_cartStack || !s_cartTcb) {
    fmt::print("[PICO8] ERROR: out of internal memory\n");
    heap_caps_free(s_audioMono); s_audioMono = nullptr;
    heap_caps_free(s_audioStereo); s_audioStereo = nullptr;
    heap_caps_free(s_cartStack); s_cartStack = nullptr;
    heap_caps_free(s_cartTcb); s_cartTcb = nullptr;
    s_quit = true;
    return;
  }

  s_emuTaskPrio = uxTaskPriorityGet(nullptr);
  vTaskPrioritySet(nullptr, EMU_TASK_PRIO_WHILE_RUNNING);

  // the interpreter's small objects come from internal SRAM first: leave the
  // emulator (menu, display path, SD) a reserve and hand Lua the rest
  {
    constexpr size_t INTERNAL_RESERVE = 48 * 1024;
    const size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t budget = freeInternal > INTERNAL_RESERVE ? freeInternal - INTERNAL_RESERVE : 0;
    p8_lua_alloc_set_internal_budget(budget);
    fmt::print("[PICO8] Lua internal-SRAM budget {} B\n", budget);
  }
  box.audio_sample_rate(SAMPLE_RATE);
  s_audioRun = true;
  // the cart on core 0 (with the emulator loop, which outranks it), audio on core 1
  s_cartTask = xTaskCreateStaticPinnedToCore(cartTask, "pico8", CART_TASK_STACK, nullptr, CART_TASK_PRIO,
                                             s_cartStack, s_cartTcb, 0);
  if (xTaskCreatePinnedToCore(audioTask, "pico8_snd", 4096, nullptr, 6, &s_audioTask, 1) != pdPASS) {
    s_audioTask = nullptr;
    s_audioRun = false;
    s_audioTaskDone = true;
  }
  if (!s_cartTask) {
    fmt::print("[PICO8] ERROR: could not start the cart task\n");
    s_quit = true;
    s_cartTaskDone = true;
  }
  s_initialized = true;
  logMemory("after init");
  reset_frame_time();
}

void reset_pico8() {
  // deinit does not touch s_cartPath, but init takes it by reference: copy first
  const std::string cart(s_cartPath);
  deinit_pico8();
  init_pico8(cart, nullptr, 0);
}

void run_pico8_rom() {
  if (!s_initialized || s_quit) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return;
  }
  const int64_t now = esp_timer_get_time();
  const unsigned presented = s_framesPresented;
  if (presented != s_lastPresented) {
    update_frame_time((now - s_lastFrameUs) / (presented - s_lastPresented));
    s_lastPresented = presented;
    s_lastFrameUs = now;
  }
  if (now - s_lastReportUs > 5000000) {
    size_t luaSmall = 0, luaLarge = 0, luaInternal = 0;
    p8_lua_alloc_stats(&luaSmall, &luaLarge, &luaInternal);
    fmt::print("[PICO8] {:.1f} fps, free internal {} B, PSRAM {} B, lua small {} B large {} B (internal slabs {} B)\n",
               (presented - s_framesAtReport) * 1000000.0 / double(now - s_lastReportUs),
               heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
               luaSmall, luaLarge, luaInternal);
#ifdef PICO8_PROFILE
    {
      // cycles spent in the PICO-8 API since the last report, by function;
      // the rest of the wall time is the Lua interpreter (and our flip)
      const double elapsedCycles = double(now - s_lastReportUs) * 240.0; // 240MHz
      uint64_t apiTotal = 0;
      int order[PICO8_PROF_MAX_REPORT];
      int n = 0;
      for (int i = 0; i < pico8_prof_count; i++) {
        apiTotal += pico8_prof[i].cycles;
        if (pico8_prof[i].cycles == 0) continue;
        if (n < PICO8_PROF_MAX_REPORT) { order[n++] = i; }
        else {
          int minIdx = 0;
          for (int k = 1; k < n; k++) if (pico8_prof[order[k]].cycles < pico8_prof[order[minIdx]].cycles) minIdx = k;
          if (pico8_prof[i].cycles > pico8_prof[order[minIdx]].cycles) order[minIdx] = i;
        }
      }
      for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++)
        if (pico8_prof[order[b]].cycles > pico8_prof[order[a]].cycles) std::swap(order[a], order[b]);
      std::string line = fmt::format("[PICO8] profile: lua {:.2f} Minstr/s, gc {:.0f}%/{} steps, api {:.0f}% ",
                                     double(pico8_prof_hooks) * 3000.0 / (double(now - s_lastReportUs) / 1e6) / 1e6,
                                     100.0 * double(pico8_prof_gc_cycles) / elapsedCycles, pico8_prof_gc_steps,
                                     100.0 * double(apiTotal) / elapsedCycles);
      pico8_prof_gc_cycles = pico8_prof_gc_steps = pico8_prof_hooks = 0;
      for (int a = 0; a < n; a++) {
        const auto& e = pico8_prof[order[a]];
        line += fmt::format("{} {:.0f}%/{} ", e.name, 100.0 * double(e.cycles) / elapsedCycles, e.calls);
      }
      fmt::print("{}\n", line);
      for (int i = 0; i < pico8_prof_count; i++) { pico8_prof[i].cycles = 0; pico8_prof[i].calls = 0; }
    }
#endif
    s_lastReportUs = now;
    s_framesAtReport = presented;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(8));
}

bool pico8_quit_requested() {
  return s_quit;
}

void pause_pico8_tasks() {
  if (!s_initialized || s_paused) {
    return;
  }
  // the cart parks itself in the pump at the next Lua hook (a few ms at most);
  // audio stops on its next chunk
  s_paused = true;
  audio_pause();
  vTaskDelay(pdMS_TO_TICKS(30));
}

void resume_pico8_tasks() {
  if (!s_initialized || !s_paused) {
    return;
  }
  // the menu overwrote the screen and palette
  auto& box = BoxEmu::get();
  box.palette(m_colors, 32);
  if (s_lastFrame) {
    box.push_frame(s_lastFrame);
  }
  audio_resume();
  s_paused = false;
}

void load_pico8(std::string_view save_path, int save_slot) {
  if (save_slot < 0) {
    return;
  }
  runStateOp(StateOp::Load, save_path);
}

void save_pico8(std::string_view save_path, int save_slot) {
  if (save_slot < 0) {
    return;
  }
  runStateOp(StateOp::Save, save_path);
}

std::span<uint8_t> get_pico8_video_buffer() {
  // RGB565 copy of the last frame (for the pause screenshot) in frame_buffer1
  uint16_t* dst = reinterpret_cast<uint16_t*>(BoxEmu::get().frame_buffer1());
  if (!s_lastFrame || !dst) {
    return std::span<uint8_t>();
  }
  for (size_t i = 0; i < FRAME_BYTES; i++) {
    dst[i] = m_colors[s_lastFrame[i] & 0x1f];
  }
  return std::span<uint8_t>(reinterpret_cast<uint8_t*>(dst), FRAME_BYTES * sizeof(uint16_t));
}

void deinit_pico8() {
  if (!s_initialized) {
    return;
  }
  s_initialized = false;
  // whatever happens below, the audio task must not touch femto8's memory
  s_audioReady = false;

  // ask the cart to unwind; the pump handles it at the next Lua hook, and
  // p8_run() then returns through p8_shutdown()
  s_stopRequested = true;
  s_paused = false;
  if (!waitFor(s_cartTaskDone, 2000)) {
    // wedged outside the Lua VM (the pump never ran): show where, then kill it.
    // femto8's buffers leak in this case (p8_shutdown never runs).
    fmt::print("[PICO8] cart task did not stop; where it is stuck:\n");
    esp_backtrace_print_all_tasks(16);
  }
  if (s_cartTask) {
    vTaskDelete(s_cartTask);
    s_cartTask = nullptr;
  }
  s_audioRun = false;
  waitFor(s_audioTaskDone, 500);
  if (s_audioTask) {
    vTaskDelete(s_audioTask);
    s_audioTask = nullptr;
  }
  vTaskDelay(pdMS_TO_TICKS(5));
  heap_caps_free(s_cartStack); s_cartStack = nullptr;
  heap_caps_free(s_cartTcb); s_cartTcb = nullptr;
  heap_caps_free(s_audioMono); s_audioMono = nullptr;
  heap_caps_free(s_audioStereo); s_audioStereo = nullptr;
  s_lastFrame = nullptr;
  s_frames[0] = s_frames[1] = nullptr;

  // femto8 is gone: its statics can be put back for the next launch
  resetStatics();

  BoxEmu::get().audio_sample_rate(48000);
  vTaskPrioritySet(nullptr, s_emuTaskPrio);
  logMemory("after deinit");
}
