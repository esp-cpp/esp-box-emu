#include "nes.hpp"

extern "C" {
#include "event.h"
#include <nes.h>
#include <nesstate.h>
}

static nes_t* console_nes;

#include <string>

#include "fs_init.hpp"
#include "format.hpp"
#include "spi_lcd.h"
#include "st7789.hpp"

static std::atomic<bool> scaled = false;
static std::atomic<bool> filled = true;

void set_nes_video_original() {
  scaled = false;
  filled = false;
  osd_set_video_scale(false);
}

void set_nes_video_fit() {
  scaled = true;
  filled = false;
  osd_set_video_scale(false);
}

void set_nes_video_fill() {
  scaled = false;
  filled = true;
  osd_set_video_scale(true);
}

void reset_nes() {
  nes_reset(SOFT_RESET);
}

static uint8_t first_frame = 0;
static uint32_t frame_counter = 0;
static float totalElapsedSeconds = 0;
void init_nes(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size) {
  static bool initialized = false;
  if (!initialized) {
    event_init();
    osd_init();
    vidinfo_t video;
    osd_getvideoinfo(&video);
    vid_init(video.default_width, video.default_height, video.driver);
    console_nes = nes_create();
    event_set_system(system_nes);
  } else {
    nes_reset(HARD_RESET);
  }
  initialized = true;
  nes_insertcart(rom_filename.c_str(), console_nes);
  vid_setmode(NES_SCREEN_WIDTH, NES_VISIBLE_HEIGHT);
  nes_prep_emulation(nullptr, console_nes);
  first_frame = 1;
  frame_counter = 0;
  totalElapsedSeconds = 0;
}

static bool load_save = false;
static std::string save_path_to_load;
void run_nes_rom() {
  if (load_save) {
    nes_prep_emulation((char *)save_path_to_load.data(), console_nes);
    load_save = false;
    frame_counter = 0;
  }
  auto start = std::chrono::high_resolution_clock::now();
  nes_emulateframe(first_frame);
  first_frame = 0;
  ++frame_counter;
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration<float>(end-start).count();
  totalElapsedSeconds += elapsed;
  if ((frame_counter % 60) == 0) {
    fmt::print("nes: FPS {}\n", (float) frame_counter / totalElapsedSeconds);
  }
  // frame rate should be 60 FPS, so 1/60th second is what we want to sleep for
  static constexpr auto delay = std::chrono::duration<float>(1.0f/60.0f);
  std::this_thread::sleep_until(start + delay);
}

void load_nes(std::string_view save_path) {
  save_path_to_load = save_path;
  load_save = true;
}

void save_nes(std::string_view save_path) {
  save_sram((char *)save_path.data(), console_nes);
}

std::vector<uint8_t> get_nes_video_buffer() {
  std::vector<uint8_t> frame(NES_SCREEN_WIDTH * NES_VISIBLE_HEIGHT * 2);
  // the frame data for the NES is stored in frame_buffer0 as a 8 bit index into the palette
  // we need to convert this to a 16 bit RGB565 value
  uint8_t *frame_buffer0 = get_frame_buffer0();
  uint16_t *palette = get_nes_palette();
  for (int i = 0; i < NES_SCREEN_WIDTH * NES_VISIBLE_HEIGHT; i++) {
    uint8_t index = frame_buffer0[i];
    uint16_t color = palette[index];
    frame[i * 2] = color & 0xFF;
    frame[i * 2 + 1] = color >> 8;
  }
  return frame;
}

void stop_nes_tasks() {
  nes_pause_video_task();
  nes_pause_audio_task();
}

void start_nes_tasks() {
  nes_resume_video_task();
  nes_resume_audio_task();
}

void deinit_nes() {
  nes_poweroff();
}
