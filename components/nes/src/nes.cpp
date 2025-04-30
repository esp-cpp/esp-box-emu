#include "nes.hpp"
#include "nes_shared_memory.h"

#include "shared_memory.h"

static nes_t* console_nes;

#include <string>

#include "box-emu.hpp"
#include "statistics.hpp"

void reset_nes() {
  nes_reset(SOFT_RESET);
}

static bool unlock = false;

static uint8_t first_frame = 0;
void init_nes(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size) {
  nes_init_shared_memory();
  event_init();
  osd_init();
  vidinfo_t video;
  osd_getvideoinfo(&video);
  vid_init(video.default_width, video.default_height, video.driver);
  nes_context = nes_create();
  console_nes = nes_context;
  event_set_system(system_nes);

  fmt::print("Num bytes allocated: {}\n", shared_num_bytes_allocated());

  // reset unlock
  unlock = false;

  // set native size
  BoxEmu::get().native_size(NES_SCREEN_WIDTH, NES_VISIBLE_HEIGHT);
  BoxEmu::get().palette(get_nes_palette());

  BoxEmu::get().audio_sample_rate(44100 / 2);

  nes_insertcart(rom_filename.c_str(), console_nes);
  vid_setmode(NES_SCREEN_WIDTH, NES_VISIBLE_HEIGHT);
  nes_prep_emulation(nullptr, console_nes);
  nes_reset(SOFT_RESET);
  first_frame = 1;
  reset_frame_time();
}

static bool load_save = false;
static std::string save_path_to_load;
void run_nes_rom() {
  if (load_save) {
    nes_prep_emulation((char *)save_path_to_load.data(), console_nes);
    load_save = false;
  }
  auto start = esp_timer_get_time();
  nes_emulateframe(first_frame);
  first_frame = 0;
  auto end = esp_timer_get_time();

  // update unlock based on x button
  auto state = BoxEmu::get().gamepad_state();
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
    std::this_thread::sleep_for(sleep_time * std::chrono::milliseconds(1));
  }
}

void load_nes(std::string_view save_path) {
  save_path_to_load = save_path;
  load_save = true;
}

void save_nes(std::string_view save_path) {
  save_sram((char *)save_path.data(), console_nes);
}

std::span<uint8_t> get_nes_video_buffer() {
  uint8_t *span_ptr = BoxEmu::get().frame_buffer1();
  std::span<uint8_t> frame(span_ptr, NES_SCREEN_WIDTH * NES_VISIBLE_HEIGHT * 2);

  // the frame data for the NES is stored in frame_buffer0 as a 8 bit index into the palette
  // we need to convert this to a 16 bit RGB565 value
  const uint8_t *frame_buffer0 = BoxEmu::get().frame_buffer0();
  const uint16_t *palette = get_nes_palette();
  for (int i = 0; i < NES_SCREEN_WIDTH * NES_VISIBLE_HEIGHT; i++) {
    uint8_t index = frame_buffer0[i];
    uint16_t color = palette[index];
    frame[i * 2] = color & 0xFF;
    frame[i * 2 + 1] = color >> 8;
  }
  return frame;
}

void deinit_nes() {
  nes_poweroff();
  BoxEmu::get().audio_sample_rate(48000);
  nes_free_shared_memory();
}
