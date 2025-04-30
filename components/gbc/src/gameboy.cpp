// #pragma GCC optimize ("Ofast")

#include "gameboy.hpp"
#include "gbc_shared_memory.hpp"
#include "shared_memory.h"

#include <memory>

#include "box-emu.hpp"
#include "statistics.hpp"

static const size_t GAMEBOY_SCREEN_WIDTH = 160;
static const size_t GAMEBOY_SCREEN_HEIGHT = 144;
static const int GAMEBOY_AUDIO_SAMPLE_RATE = 32000;

extern "C" {
#include <gnuboy/loader.h>
#include <gnuboy/hw.h>
#include <gnuboy/lcd.h>
#include <gnuboy/fb.h>
#include <gnuboy/cpu.h>
#include <gnuboy/mem.h>
#include <gnuboy/sound.h>
#include <gnuboy/pcm.h>
#include <gnuboy/regs.h>
#include <gnuboy/rtc.h>
#include <gnuboy/gnuboy.h>
}

using namespace std::chrono_literals;

// need to have these here for gnuboy to work
uint32_t frame = 0;
uint16_t* displayBuffer[2];

static uint16_t* framebuffer;
struct fb fb;
struct pcm pcm;
static uint8_t currentBuffer = 0;

// cppcheck-suppress constParameterPointer
extern "C" void die(char *fmt, ...) {
  // do nothing...
}

void run_to_vblank() {
  /* FRAME BEGIN */
  /* FIXME: judging by the time specified this was intended
  to emulate through vblank phase which is handled at the
  end of the loop. */
  cpu_emulate(2280);
  // cpu_emulate(2280 / 2);

  /* FIXME: R_LY >= 0; comparsion to zero can also be removed
  altogether, R_LY is always 0 at this point */
  while (R_LY > 0 && R_LY < GAMEBOY_SCREEN_HEIGHT)
  {
    emu_step();
  }

  /* VBLANK BEGIN */
  if ((frame % 2) == 0) {
    BoxEmu::get().push_frame(framebuffer);

    // swap buffers
    currentBuffer = currentBuffer ? 0 : 1;
    framebuffer = displayBuffer[currentBuffer];
    fb.ptr = (uint8_t*)framebuffer;
  }

  rtc_tick();

  sound_mix();

  if (pcm.pos > 100) {
    auto audio_sample_count = pcm.pos;
    auto audio_buffer = (uint8_t*)pcm.buf;
    BoxEmu::get().play_audio(audio_buffer, audio_sample_count * sizeof(int16_t));
    pcm.pos = 0;
  }

  if (!(R_LCDC & 0x80)) {
    /* LCDC operation stopped */
    /* FIXME: judging by the time specified, this is
    intended to emulate through visible line scanning
    phase, even though we are already at vblank here */
    // cpu_emulate(32832); // TODO: this was the original, but WHY?
    cpu_emulate(32832 / 3);
    // cpu_emulate(32832 / 4);
  }

  while (R_LY > 0) {
    /* Step through vblank phase */
    emu_step();
  }
  ++frame;
}

void reset_gameboy() {
  emu_reset();
}

static bool unlock = false;

void init_gameboy(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size) {
  // Initialize shared memory
  gbc_init_shared_memory();

  // Get shared memory regions
  uint8_t* vram = nullptr;
  uint8_t* wram = nullptr;
  uint8_t* audio = nullptr;
  gbc_get_memory_regions(&vram, &wram, &audio);

  // Use shared memory regions
  lcd->vbank = vram;
  ram.ibank = wram;
  pcm.buf = (int16_t*)audio;
  static constexpr int GBC_AUDIO_BUFFER_SIZE = GAMEBOY_AUDIO_SAMPLE_RATE * 2 * 2 / 5; // TODO: 5 is a hack to make it work
  pcm.len = GBC_AUDIO_BUFFER_SIZE / sizeof(int16_t);

  // set native size
  BoxEmu::get().native_size(GAMEBOY_SCREEN_WIDTH, GAMEBOY_SCREEN_HEIGHT);
  BoxEmu::get().palette(nullptr);

  displayBuffer[0] = (uint16_t*)BoxEmu::get().frame_buffer0();
  displayBuffer[1] = (uint16_t*)BoxEmu::get().frame_buffer1();

  memset(&fb, 0, sizeof(fb));
  fb.w = GAMEBOY_SCREEN_WIDTH;
  fb.h = GAMEBOY_SCREEN_HEIGHT;
  fb.pelsize = 2;
  fb.pitch = fb.w * fb.pelsize;
  fb.indexed = 0;
  fb.ptr = (uint8_t*)displayBuffer[0];
  fb.enabled = 1;
  fb.dirty = 0;
  framebuffer = displayBuffer[0];

  BoxEmu::get().audio_sample_rate(GAMEBOY_AUDIO_SAMPLE_RATE);
  // save the audio buffer
  auto buf = pcm.buf;
  auto len = pcm.len;
  memset(&pcm, 0, sizeof(pcm));
  pcm.hz = 32000;
  pcm.stereo = 1;
  pcm.len = len;
  pcm.buf = buf;
  pcm.pos = 0;

  // reset unlock
  unlock = false;

  sound_reset();

  loader_init(romdata, rom_data_size);
  emu_reset();
  frame = 0;
  reset_frame_time();
}

void run_gameboy_rom() {
  auto start = esp_timer_get_time();
  // GET INPUT
  auto state = BoxEmu::get().gamepad_state();
  pad_set(PAD_UP, state.up);
  pad_set(PAD_DOWN, state.down);
  pad_set(PAD_LEFT, state.left);
  pad_set(PAD_RIGHT, state.right);
  pad_set(PAD_SELECT, state.select);
  pad_set(PAD_START, state.start);
  pad_set(PAD_A, state.a);
  pad_set(PAD_B, state.b);
  run_to_vblank();
  auto end = esp_timer_get_time();

  // update unlock based on x button
  static bool last_x = false;
  if (state.x && !last_x) {
    unlock = !unlock;
  }
  last_x = state.x;

  auto elapsed = end - start;
  update_frame_time(elapsed);
  static constexpr uint64_t max_frame_time = 1000000 / 60;
  if (!unlock && elapsed < max_frame_time) {
    auto sleep_time = (max_frame_time - elapsed) / 1e3;
    std::this_thread::sleep_for(sleep_time * 1ms);
  }
}

void load_gameboy(std::string_view save_path) {
  if (save_path.size()) {
    auto f = fopen(save_path.data(), "rb");
    loadstate(f);
    fclose(f);
    vram_dirty();
    pal_dirty();
    sound_dirty();
    mem_updatemap();
  }
}

void save_gameboy(std::string_view save_path) {
  // save state
  auto f = fopen(save_path.data(), "wb");
  savestate(f);
  fclose(f);
}

std::span<uint8_t> get_gameboy_video_buffer() {
  return std::span<uint8_t>((uint8_t*)framebuffer, GAMEBOY_SCREEN_WIDTH * GAMEBOY_SCREEN_HEIGHT * 2);
}

void deinit_gameboy() {
  // Free shared memory
  gbc_free_shared_memory();

  // now unload everything
  loader_unload();
  BoxEmu::get().audio_sample_rate(48000);
}
