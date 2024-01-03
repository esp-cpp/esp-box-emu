// #pragma GCC optimize ("Ofast")

#include "gameboy.hpp"

#include <memory>

#include "fs_init.hpp"
#include "statistics.hpp"

#include "format.hpp"
#include "spi_lcd.h"
#include "i2s_audio.h"
#include "input.h"
#include "st7789.hpp"
#include "task.hpp"
#include "video_task.hpp"
#include "audio_task.hpp"

static const size_t GAMEBOY_SCREEN_WIDTH = 160;
static const size_t GAMEBOY_SCREEN_HEIGHT = 144;

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

// need to have these haere for gnuboy to work
uint32_t frame = 0;
uint16_t* displayBuffer[2];

static uint16_t* framebuffer;
struct fb fb;
struct pcm pcm;
static uint8_t currentBuffer = 0;

static int32_t* audioBuffer[2];
volatile uint8_t currentAudioBuffer = 0;
volatile uint16_t currentAudioSampleCount;
volatile int32_t* currentAudioBufferPtr;

extern "C" void die(char *fmt, ...) {
  // do nothing...
}

void run_to_vblank() {
  /* FRAME BEGIN */
  auto start = std::chrono::high_resolution_clock::now();

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
    hal::push_frame(framebuffer);

    // swap buffers
    currentBuffer = currentBuffer ? 0 : 1;
    framebuffer = displayBuffer[currentBuffer];
    fb.ptr = (uint8_t*)framebuffer;
  }

  rtc_tick();

  sound_mix();

  if (pcm.pos > 100) {
    currentAudioBufferPtr = audioBuffer[currentAudioBuffer];
    currentAudioSampleCount = pcm.pos;

    hal::set_audio_sample_count(currentAudioSampleCount);
    hal::push_audio((const void*)currentAudioBufferPtr);

    // Swap buffers
    currentAudioBuffer = currentAudioBuffer ? 0 : 1;
    pcm.buf = (int16_t*)audioBuffer[currentAudioBuffer];
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
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration<float>(end-start).count();
  update_frame_time(elapsed);
  // frame rate should be 60 FPS, so 1/60th second is what we want to sleep for
  static constexpr auto delay = std::chrono::duration<float>(1.0f/60.0f);
  std::this_thread::sleep_until(start + delay);
}

void reset_gameboy() {
  emu_reset();
}

void init_gameboy(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size) {
  // set native size
  hal::set_native_size(GAMEBOY_SCREEN_WIDTH, GAMEBOY_SCREEN_HEIGHT);
  hal::set_palette(nullptr);

  displayBuffer[0] = (uint16_t*)get_frame_buffer0();
  displayBuffer[1] = (uint16_t*)get_frame_buffer1();
  audioBuffer[0] = (int32_t*)get_audio_buffer0();
  audioBuffer[1] = (int32_t*)get_audio_buffer1();

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

  memset(&pcm, 0, sizeof(pcm));
  pcm.hz = 16000;
  pcm.stereo = 1;
  pcm.len = AUDIO_BUFFER_SIZE;
  pcm.buf = (int16_t*)audioBuffer[0];
  pcm.pos = 0;

  sound_reset();

  loader_init(romdata, rom_data_size);
  emu_reset();
  frame = 0;
  reset_frame_time();
}

void run_gameboy_rom() {
  // GET INPUT
  InputState state;
  get_input_state(&state);
  pad_set(PAD_UP, state.up);
  pad_set(PAD_DOWN, state.down);
  pad_set(PAD_LEFT, state.left);
  pad_set(PAD_RIGHT, state.right);
  pad_set(PAD_SELECT, state.select);
  pad_set(PAD_START, state.start);
  pad_set(PAD_A, state.a);
  pad_set(PAD_B, state.b);
  run_to_vblank();
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

void stop_gameboy_tasks() {
}

void start_gameboy_tasks() {
}

std::vector<uint8_t> get_gameboy_video_buffer() {
  const uint8_t* frame_buffer = get_frame_buffer0();
  // copy the frame buffer to a new buffer
  auto width = GAMEBOY_SCREEN_WIDTH;
  auto height = GAMEBOY_SCREEN_HEIGHT;
  std::vector<uint8_t> new_frame_buffer(width * 2 * height);
  for (int y = 0; y < height; ++y) {
    memcpy(&new_frame_buffer[y * width * 2], &frame_buffer[y * width * 2], width * 2);
  }
  return new_frame_buffer;
}

void deinit_gameboy() {
  // now unload everything
  loader_unload();
}
