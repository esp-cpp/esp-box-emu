// Metal Gear Solid glue for esp-box-emu.
//
// The decompiled game is not an emulated ROM: it is native code with its own
// cooperative scheduler (mts) mapped onto FreeRTOS tasks by mgs/port. So
// unlike the other cores there is no "run one frame" entry point; init_mgs()
// starts the game in its own task and run_mgs_rom() only feeds it the gamepad.
//
// The game assumes it owns the machine (the PSX booted from scratch every
// time), so stopping it is the hard part:
//  * its three big buffers (VRAM, main RAM, face group) are carved out of the
//    emulator's 4MB ROM block through the pool allocator rather than being
//    statics;
//  * every other static of libmgs.a (game, psyz, PSY-Q, port) is bracketed by
//    linker symbols (linker.lf): .bss/.common are zeroed and .data restored
//    from a snapshot taken before the first launch, so a relaunch sees exactly
//    the state the first launch did. This file's own statics are kept out of
//    that range (see linker.lf).
#include "mgs.hpp"

#include "box-emu.hpp"
#include "statistics.hpp"
#include "pool_allocator.h"
#include "platform/mgs_glue.h"

#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <chrono>
#include <cstring>
#include <thread>

extern "C" {
// linker.lf SURROUND symbols for libmgs.a's statics
extern char _mgs_bss_start[], _mgs_bss_end[];
extern char _mgs_common_start[], _mgs_common_end[];
extern char _mgs_data_start[], _mgs_data_end[];
// the game's big buffers (pointers under MGS_ESPBOX, see mgs/port/psyz_port.c
// and mgs/port/soft_render.c)
extern unsigned char* mgs_main_ram;
extern unsigned char* mgs_face_group;
extern unsigned short* g_RawVram;
// mts scheduler state (mgs/source/mts/mts_new.c, psyz libapi.c)
extern int mts_active_task_800C0DB0;
extern volatile int psyz_critical_depth;
extern unsigned mgs_frame_seq;
}

namespace {
  constexpr int MGS_W = 320;
  constexpr int MGS_H = 240;
  constexpr int MTS_TASK_IDLE = 11;
  constexpr size_t ROM_POOL_SIZE = 4 * 1024 * 1024;
  // the task that runs mgs_main(); its stack has to be internal (a PSRAM stack
  // dies the first time the flash cache is disabled for an SD read)
  constexpr size_t GAME_TASK_STACK = 16 * 1024;
  constexpr UBaseType_t GAME_TASK_PRIO = 5; // same as the mts threads it spawns
  // the emulator loop must still get the CPU while the game saturates core 0
  constexpr UBaseType_t EMU_TASK_PRIO_WHILE_RUNNING = 6;

  std::string s_dataRoot;
  bool s_initialized = false;
  bool s_paused = false;
  bool s_quit = false;
  uint8_t* s_dataSnapshot = nullptr; // .data of libmgs.a as linked; survives relaunches

  TaskHandle_t s_gameTask = nullptr;
  StackType_t* s_gameStack = nullptr;
  StaticTask_t* s_gameTcb = nullptr;
  UBaseType_t s_emuTaskPrio = 1;

  uint16_t* s_presentBuffers[2] = {nullptr, nullptr};
  const uint16_t* s_lastFrame = nullptr;
  unsigned s_lastPresented = 0;
  int64_t s_lastFrameUs = 0;

  void logMemory(const char* when) {
    fmt::print("[MGS] {}: free internal {} B (largest {} B), free PSRAM {} B (largest {} B)\n", when,
               heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
               heap_caps_get_free_size(MALLOC_CAP_SPIRAM), heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
  }

  // Put every static of libmgs.a back to its as-linked value. Only valid while
  // none of the game's tasks exist.
  void resetStatics() {
    const size_t dataSize = _mgs_data_end - _mgs_data_start;
    if (!s_dataSnapshot) {
      // first launch: the sections are pristine, remember .data
      s_dataSnapshot = (uint8_t*)heap_caps_malloc(dataSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (s_dataSnapshot) {
        memcpy(s_dataSnapshot, _mgs_data_start, dataSize);
      }
      fmt::print("[MGS] statics: bss {} B, common {} B, data {} B (snapshot {})\n",
                 _mgs_bss_end - _mgs_bss_start, _mgs_common_end - _mgs_common_start, dataSize,
                 s_dataSnapshot ? "ok" : "FAILED");
      return;
    }
    memset(_mgs_bss_start, 0, _mgs_bss_end - _mgs_bss_start);
    memset(_mgs_common_start, 0, _mgs_common_end - _mgs_common_start);
    memcpy(_mgs_data_start, s_dataSnapshot, dataSize);
  }

  void gameTask(void*) {
    fmt::print("[MGS] game task started on core {}\n", xPortGetCoreID());
    mgs_main();
    // Main() never returns on the console; if it does here, the game is over.
    fmt::print("[MGS] mgs_main() returned\n");
    s_quit = true;
    vTaskSuspend(nullptr);
    for (;;) {
      vTaskDelay(portMAX_DELAY);
    }
  }

  // Map the box gamepad onto the PSX pad (libgv.h PAD_* bit layout). SELECT is
  // a modifier for the shoulder buttons, which the box does not have.
  void updateInput() {
    const GamepadState state = BoxEmu::get().gamepad_state();
    uint16_t pad = 0;
    if (state.up)    pad |= 0x1000;
    if (state.down)  pad |= 0x4000;
    if (state.left)  pad |= 0x8000;
    if (state.right) pad |= 0x2000;
    if (state.start) pad |= 0x0800;
    if (state.select) {
      // SELECT + face button -> shoulder button
      if (state.a) pad |= 0x0004; // L1
      if (state.b) pad |= 0x0008; // R1
      if (state.x) pad |= 0x0001; // L2
      if (state.y) pad |= 0x0002; // R2
      // SELECT alone (no combo) is the PSX SELECT button
      if (!(state.a || state.b || state.x || state.y)) pad |= 0x0100;
    } else {
      if (state.a) pad |= 0x0020; // CIRCLE
      if (state.b) pad |= 0x0040; // CROSS
      if (state.x) pad |= 0x0080; // SQUARE
      if (state.y) pad |= 0x0010; // TRIANGLE
    }
    mgs_box_pad = pad;
  }

  // Wait (up to `maxMs`) for the mts scheduler to reach its idle task outside
  // a critical section: at that point no game thread holds a lock (file, heap,
  // stdio) and the whole thing can be frozen or torn down safely.
  bool waitForIdle(int maxMs) {
    for (int waited = 0; waited < maxMs; waited += 2) {
      if (mts_active_task_800C0DB0 == MTS_TASK_IDLE && psyz_critical_depth == 0) {
        return true;
      }
      vTaskDelay(pdMS_TO_TICKS(2));
    }
    return false;
  }

  // Freeze the game at a quiescent point: hold the vblank tick first (nothing
  // can wake an mts thread without it), then the running thread.
  void freezeGame() {
    const bool idle = waitForIdle(500);
    Mgs_PauseVblank();
    if (!idle && !(mts_active_task_800C0DB0 == MTS_TASK_IDLE && psyz_critical_depth == 0)) {
      // the tick is held now; give the running thread a last chance to park
      waitForIdle(200);
    }
    Mgs_ThreadsPause();
    if (s_gameTask && mts_active_task_800C0DB0 != MTS_TASK_IDLE) {
      fmt::print("[MGS] freeze: active mts task {} (not idle)\n", mts_active_task_800C0DB0);
    }
  }
}

extern "C" void mgs_present_frame(const uint16_t* frame) {
  s_lastFrame = frame;
  if (!s_paused) {
    BoxEmu::get().push_frame(frame);
  }
}

void init_mgs(const std::string& rom_filename, uint8_t* romdata, size_t rom_data_size) {
  (void)romdata; (void)rom_data_size;
  if (s_initialized) {
    return;
  }
  s_quit = false;
  s_paused = false;
  mgs_box_pad = 0;

  // STAGE.DIR names the data directory; everything else lives next to it
  const size_t slash = rom_filename.find_last_of('/');
  s_dataRoot = (slash == std::string::npos) ? std::string("/sdcard/mgs") : rom_filename.substr(0, slash);
  fmt::print("[MGS] data root: {}\n", s_dataRoot);
  logMemory("before init");

  resetStatics();

  auto& box = BoxEmu::get();
  // The game reads from the card, so the 4MB ROM block is free for its buffers.
  pool_create(box.romdata(), ROM_POOL_SIZE);
  g_RawVram = (unsigned short*)pool_alloc(MGS_VRAM_BYTES);
  mgs_main_ram = (unsigned char*)pool_alloc(MGS_MAIN_RAM_BYTES);
  mgs_face_group = (unsigned char*)pool_alloc(MGS_FACE_GROUP_BYTES);
  s_presentBuffers[0] = (uint16_t*)pool_alloc(MGS_W * MGS_H * sizeof(uint16_t));
  s_presentBuffers[1] = (uint16_t*)pool_alloc(MGS_W * MGS_H * sizeof(uint16_t));
  s_gameStack = (StackType_t*)heap_caps_malloc(GAME_TASK_STACK, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  s_gameTcb = (StaticTask_t*)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!g_RawVram || !mgs_main_ram || !mgs_face_group || !s_presentBuffers[0] || !s_presentBuffers[1] ||
      !s_gameStack || !s_gameTcb) {
    fmt::print("[MGS] ERROR: out of memory (vram {} ram {} face {} present {} {} stack {} tcb {})\n",
               (void*)g_RawVram, (void*)mgs_main_ram, (void*)mgs_face_group, (void*)s_presentBuffers[0],
               (void*)s_presentBuffers[1], (void*)s_gameStack, (void*)s_gameTcb);
    heap_caps_free(s_gameStack); s_gameStack = nullptr;
    heap_caps_free(s_gameTcb); s_gameTcb = nullptr;
    g_RawVram = nullptr; mgs_main_ram = nullptr; mgs_face_group = nullptr;
    s_presentBuffers[0] = s_presentBuffers[1] = nullptr;
    pool_destroy();
    s_quit = true;
    return;
  }
  memset(g_RawVram, 0, MGS_VRAM_BYTES);
  memset(mgs_main_ram, 0, MGS_MAIN_RAM_BYTES);
  memset(mgs_face_group, 0, MGS_FACE_GROUP_BYTES);
  memset(s_presentBuffers[0], 0, MGS_W * MGS_H * sizeof(uint16_t));
  memset(s_presentBuffers[1], 0, MGS_W * MGS_H * sizeof(uint16_t));
  // the rasterizer caches the VRAM pointer (soft_render.c binds it from a
  // constructor, which ran long before this allocation existed)
  Draw_Reset();
  mgs_present_buffers[0] = s_presentBuffers[0];
  mgs_present_buffers[1] = s_presentBuffers[1];
  s_lastFrame = nullptr;
  s_lastPresented = mgs_presented_frames;
  s_lastFrameUs = esp_timer_get_time();

  // the presenter hands us finished RGB565 frames; no palette, no scaling
  box.palette(nullptr);
  box.native_size(MGS_W, MGS_H, MGS_W);

  // keep the emulator loop (this task, core 0) ahead of the game's threads
  s_emuTaskPrio = uxTaskPriorityGet(nullptr);
  vTaskPrioritySet(nullptr, EMU_TASK_PRIO_WHILE_RUNNING);

  Mgs_SetDataRoot(s_dataRoot.c_str());
  Mgs_CdInit();
  Mgs_StartVblank();
  s_gameTask = xTaskCreateStaticPinnedToCore(gameTask, "mgs_main", GAME_TASK_STACK, nullptr, GAME_TASK_PRIO,
                                             s_gameStack, s_gameTcb, 0);
  if (!s_gameTask) {
    fmt::print("[MGS] ERROR: could not start the game task\n");
    s_quit = true;
  }
  s_initialized = true;
  logMemory("after init");
  reset_frame_time();
}

void reset_mgs() {
  const std::string stageDir = s_dataRoot + "/STAGE.DIR";
  deinit_mgs();
  init_mgs(stageDir, nullptr, 0);
}

void run_mgs_rom() {
  if (!s_initialized || s_quit) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return;
  }
  updateInput();
  // frame statistics from the presenter (one entry per frame the game flipped)
  const unsigned presented = mgs_presented_frames;
  const int64_t now = esp_timer_get_time();
  if (presented != s_lastPresented) {
    const unsigned n = presented - s_lastPresented;
    update_frame_time((now - s_lastFrameUs) / n);
    s_lastPresented = presented;
    s_lastFrameUs = now;
  }
  {
    static int64_t lastReport = 0;
    static unsigned framesAtReport = 0;
    if (now - lastReport > 5000000) {
      if (lastReport) {
        fmt::print("[MGS] {:.1f} fps, free internal {} B, PSRAM {} B, mts active {}\n",
                   (presented - framesAtReport) * 1000000.0 / double(now - lastReport),
                   heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                   mts_active_task_800C0DB0);
      }
      lastReport = now;
      framesAtReport = presented;
    }
  }
  // the pad is sampled by the vblank tick every 16ms; poll a little faster
  std::this_thread::sleep_for(std::chrono::milliseconds(8));
}

bool mgs_quit_requested() {
  return s_quit;
}

void pause_mgs_tasks() {
  if (!s_initialized || s_paused) {
    return;
  }
  freezeGame();
  s_paused = true;
}

void resume_mgs_tasks() {
  if (!s_initialized || !s_paused) {
    return;
  }
  s_paused = false;
  // the emulator menu overwrote the screen; show the last frame again
  BoxEmu::get().palette(nullptr);
  if (s_lastFrame) {
    BoxEmu::get().push_frame(s_lastFrame);
  }
  Mgs_ThreadsResume();
  Mgs_ResumeVblank();
}

void load_mgs(std::string_view, int) {
  fmt::print("[MGS] save states are not supported\n");
}

void save_mgs(std::string_view, int) {
  fmt::print("[MGS] save states are not supported\n");
}

std::span<uint8_t> get_mgs_video_buffer() {
  // the presenter's last RGB565 frame (for the pause screenshot)
  if (s_lastFrame) {
    return std::span<uint8_t>((uint8_t*)s_lastFrame, MGS_W * MGS_H * sizeof(uint16_t));
  }
  return std::span<uint8_t>();
}

void deinit_mgs() {
  if (!s_initialized) {
    return;
  }
  s_initialized = false;
  // Stop at a quiescent point (see freezeGame); if we were paused by the menu
  // the game is already frozen there.
  if (!s_paused) {
    freezeGame();
  }
  s_paused = true;
  // nothing may wake an mts thread from here on
  Mgs_StopVblank();
  Mgs_ThreadsStopAll();
  if (s_gameTask) {
    vTaskDelete(s_gameTask);
    s_gameTask = nullptr;
  }
  // let the scheduler retire the deleted tasks before their memory goes away
  vTaskDelay(pdMS_TO_TICKS(5));
  heap_caps_free(s_gameStack); s_gameStack = nullptr;
  heap_caps_free(s_gameTcb); s_gameTcb = nullptr;
  Mgs_CdDeinit();

  mgs_present_buffers[0] = mgs_present_buffers[1] = nullptr;
  s_lastFrame = nullptr;
  g_RawVram = nullptr;
  mgs_main_ram = nullptr;
  mgs_face_group = nullptr;
  s_presentBuffers[0] = s_presentBuffers[1] = nullptr;
  pool_destroy();

  // the game is gone: its statics can be put back for the next launch
  resetStatics();

  vTaskPrioritySet(nullptr, s_emuTaskPrio);
  logMemory("after deinit");
}
