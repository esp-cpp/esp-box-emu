#pragma once

#include <memory>
#include "format.hpp"
#include "spi_lcd.h"
#include "i2s_audio.h"
#include "input.h"
#include "st7789.hpp"

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

static const size_t gameboy_screen_width = 160;

uint16_t* displayBuffer[2];
struct fb fb;
struct pcm pcm;
uint8_t currentBuffer;
uint16_t* framebuffer;
int frame = 0;
uint elapsedTime = 0;

#define AUDIO_SAMPLE_RATE (16000)

int32_t* audioBuffer[2];
volatile uint8_t currentAudioBuffer = 0;
volatile uint16_t currentAudioSampleCount;
volatile int32_t* currentAudioBufferPtr;

extern "C" void die(char *fmt, ...) {
  fmt::print("DIE!\n");
}

void run_to_vblank()
{
  /* FRAME BEGIN */

  /* FIXME: djudging by the time specified this was intended
  to emulate through vblank phase which is handled at the
  end of the loop. */
  cpu_emulate(2280);

  /* FIXME: R_LY >= 0; comparsion to zero can also be removed
  altogether, R_LY is always 0 at this point */
  while (R_LY > 0 && R_LY < 144)
  {
    emu_step();
  }

  /* VBLANK BEGIN */

  //vid_end();
  if ((frame % 2) == 0) {
    auto _frame = displayBuffer[currentBuffer];
    for (int y=0; y<144; y+=48) {
      lcd_write_frame(0, y, 160, 48, (uint8_t*)&_frame[y*160]);
    }

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

    // void* tempPtr = 0x1234;
    audio_play_frame((uint8_t*)currentAudioBufferPtr, currentAudioSampleCount * 2);

    // Swap buffers
    currentAudioBuffer = currentAudioBuffer ? 0 : 1;
    pcm.buf = (int16_t*)audioBuffer[currentAudioBuffer];
    pcm.pos = 0;
  }

  if (!(R_LCDC & 0x80)) {
    /* LCDC operation stopped */
    /* FIXME: djudging by the time specified, this is
    intended to emulate through visible line scanning
    phase, even though we are already at vblank here */
    cpu_emulate(32832);
  }

  while (R_LY > 0) {
    /* Step through vblank phase */
    emu_step();
  }
  ++frame;
}

void init_gameboy(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size) {
  // WIDTH = gameboy_screen_width, so 320-WIDTH is gameboy_screen_width
  espp::St7789::set_offset((320-gameboy_screen_width) / 2, (240-144) / 2);
  static bool initialized = false;

  // Note: Magic number obtained by adjusting until audio buffer overflows stop.
  const int audioBufferLength = AUDIO_SAMPLE_RATE / 5 + 1; //  / 10
  //printf("CHECKPOINT AUDIO: HEAP:0x%x - allocating 0x%x\n", esp_get_free_heap_size(), audioBufferLength * sizeof(int16_t) * 2 * 2);
  const int AUDIO_BUFFER_SIZE = audioBufferLength * sizeof(int16_t) * 2;

  if (!initialized) {
    displayBuffer[0] = (uint16_t*)heap_caps_malloc(160*144*2, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    displayBuffer[1] = (uint16_t*)heap_caps_malloc(160*144*2, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    audioBuffer[0] = (int32_t*)heap_caps_malloc(AUDIO_BUFFER_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
    audioBuffer[1] = (int32_t*)heap_caps_malloc(AUDIO_BUFFER_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
  }
  memset(&fb, 0, sizeof(fb));
  fb.w = 160;
  fb.h = 144;
  fb.pelsize = 2;
  fb.pitch = fb.w * fb.pelsize;
  fb.indexed = 0;
  fb.ptr = (uint8_t*)displayBuffer[0];
  fb.enabled = 1;
  fb.dirty = 0;


  // pcm.len = count of 16bit samples (x2 for stereo)
  memset(&pcm, 0, sizeof(pcm));
  pcm.hz = AUDIO_SAMPLE_RATE;
  pcm.stereo = 1;
  pcm.len = /*pcm.hz / 2*/ audioBufferLength;
  pcm.buf = (int16_t*)audioBuffer[0];
  pcm.pos = 0;

  sound_reset();

  fmt::print("GAMEBOY enabled: GNUBOY\n");
  loader_init(romdata, rom_data_size);
  emu_reset();
  initialized = true;
}

void run_gameboy_rom() {
  uint8_t _num_touches, _btn_state;
  uint16_t _x,_y;
  touchpad_read(&_num_touches, &_x, &_y, &_btn_state);
  static size_t frame = 0;
  static auto start = std::chrono::high_resolution_clock::now();
  frame++;
  if ((frame % 60) == 0) {
    auto end = std::chrono::high_resolution_clock::now();
    float elapsed = std::chrono::duration<float>(end - start).count();
    fmt::print("gameboy: FPS {}\n", (float) frame / elapsed);
  }

  run_to_vblank();
}

void deinit_gameboy() {
  fmt::print("quitting gameboy emulation!\n");
  loader_unload();
}
