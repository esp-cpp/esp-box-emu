#include "sms.hpp"

extern "C" {
#include "smsplus/shared.h"
};

#include <string>

#include "box-emu.hpp"
#include "statistics.hpp"

static constexpr size_t SMS_SCREEN_WIDTH = 256;
static constexpr size_t SMS_VISIBLE_HEIGHT = 192;

static constexpr size_t GG_SCREEN_WIDTH = 160;
static constexpr size_t GG_VISIBLE_HEIGHT = 144;

static uint16_t palette[PALETTE_SIZE];

static constexpr size_t SRAM_SIZE = 0x8000;
static constexpr size_t RAM_SIZE = 0x2000;
static constexpr size_t VRAM_SIZE = 0x4000;
static constexpr size_t AUDIO_SIZE = 0x10000;

static uint8_t *sms_sram = nullptr;
static uint8_t *sms_ram = nullptr;
static uint8_t *sms_vdp_vram = nullptr;

static int frame_buffer_offset = 0;
static int frame_buffer_size = 256*192;
static bool is_gg = false;

static int frame_counter = 0;
static uint16_t muteFrameCount = 0;
static int frame_buffer_index = 0;

static uint32_t *sms_audio_buffer = nullptr;

static bool unlock = false;

void sms_frame(int skip_render);
void sms_init(void);
void sms_reset(void);

// cppcheck-suppress constParameterPointer
extern "C" void system_manage_sram(uint8 *sram, int slot, int mode) {
}

void reset_sms() {
  system_reset();
  frame_counter = 0;
  muteFrameCount = 0;
}

static void init_memory() {
  // Use shared memory regions instead of dynamic allocation
  sms_sram = (uint8_t*)shared_malloc(SRAM_SIZE);
  sms_ram = (uint8_t*)shared_malloc(RAM_SIZE);
  sms_vdp_vram = (uint8_t*)shared_malloc(VRAM_SIZE);
  sms_audio_buffer = (uint32_t*)shared_malloc(AUDIO_SIZE);
  Z80 = (Z80_Regs*)shared_malloc(sizeof(Z80_Regs));
  io_lut = (io_state (*)[256])shared_malloc(sizeof(io_state[2][256]));
  sms = (sms_t*)shared_malloc(sizeof(sms_t));
  bios = (bios_t*)shared_malloc(sizeof(bios_t));
  slot = (slot_t*)shared_malloc(sizeof(slot_t));
  coleco = (t_coleco*)shared_malloc(sizeof(t_coleco));
  dummy_write = (uint8_t*)shared_malloc(0x400);
  dummy_read = (uint8_t*)shared_malloc(0x400);
  vdp = (vdp_t*)shared_malloc(sizeof(vdp_t));
  internal_buffer = (uint8_t*)shared_malloc(0x200);
  object_info = (struct obj_info_t*)shared_malloc(sizeof(struct obj_info_t) * 64);
  sms_snd = (sms_snd_t*)shared_malloc(sizeof(sms_snd_t));
  SN76489 = (SN76489_Context*)shared_malloc(sizeof(SN76489_Context) * MAX_SN76489);

  fmt::print("Num bytes allocated: {}\n", shared_num_bytes_allocated());
}

static void init(uint8_t *romdata, size_t rom_data_size) {
  bitmap.width = SMS_SCREEN_WIDTH;
  bitmap.height = SMS_VISIBLE_HEIGHT;
  bitmap.pitch = bitmap.width;
  bitmap.data = BoxEmu::get().frame_buffer0();

  cart.sram = sms_sram;
  sms->wram = sms_ram;
  sms->use_fm = 0;
  vdp->vram = sms_vdp_vram;

  set_option_defaults();

  option.sndrate = BoxEmu::get().audio_sample_rate();
  option.overscan = 0;
  option.extra_gg = 0;

  system_init2();
  system_reset();

  frame_counter = 0;
  muteFrameCount = 0;

  // reset unlock
  unlock = false;

  reset_frame_time();
}

void init_sms(uint8_t *romdata, size_t rom_data_size) {
  is_gg = false;
  BoxEmu::get().native_size(SMS_SCREEN_WIDTH, SMS_VISIBLE_HEIGHT, SMS_SCREEN_WIDTH);
  init_memory();
  load_rom_data(romdata, rom_data_size);
  sms->console = CONSOLE_SMS;
  sms->display = DISPLAY_NTSC;
  sms->territory = TERRITORY_DOMESTIC;
  frame_buffer_offset = 0;
  frame_buffer_size = SMS_SCREEN_WIDTH * SMS_VISIBLE_HEIGHT;
  init(romdata, rom_data_size);
  fmt::print("sms init done\n");
}

void init_gg(uint8_t *romdata, size_t rom_data_size) {
  is_gg = true;
  BoxEmu::get().native_size(GG_SCREEN_WIDTH, GG_VISIBLE_HEIGHT, SMS_SCREEN_WIDTH);
  init_memory();
  load_rom_data(romdata, rom_data_size);
  sms->console = CONSOLE_GG;
  sms->display = DISPLAY_NTSC;
  sms->territory = TERRITORY_DOMESTIC;
  frame_buffer_offset = 48; // from odroid_display.cpp lines 1055
  frame_buffer_size = GG_SCREEN_WIDTH * GG_VISIBLE_HEIGHT;
  init(romdata, rom_data_size);
  fmt::print("gg init done\n");
}

void run_sms_rom() {
  auto start = esp_timer_get_time();
  // handle input here (see system.h and use input.pad and input.system)
  auto state = BoxEmu::get().gamepad_state();

  // pad[0] is player 0
  input.pad[0] = 0;
  input.pad[0]|= state.up ? INPUT_UP : 0;
  input.pad[0]|= state.down ? INPUT_DOWN : 0;
  input.pad[0]|= state.left ? INPUT_LEFT : 0;
  input.pad[0]|= state.right ? INPUT_RIGHT : 0;
  input.pad[0]|= state.a ? INPUT_BUTTON2 : 0;
  input.pad[0]|= state.b ? INPUT_BUTTON1 : 0;

  // pad[1] is player 1
  input.pad[1] = 0;

  // system is where we input the start button, as well as soft reset
  input.system = 0;
  input.system |= state.start ? INPUT_START : 0;
  input.system |= state.select ? INPUT_PAUSE : 0;

  // emulate the frame
  if (0 || (frame_counter % 2) == 0) {
    memset(bitmap.data, 0, frame_buffer_size);
    system_frame(0);

    // copy the palette
    render_copy_palette(palette);
    // flip the bytes in the palette
    for (int i = 0; i < PALETTE_SIZE; i++) {
      uint16_t rgb565 = palette[i];
      palette[i] = (rgb565 >> 8) | (rgb565 << 8);
    }
    // set the palette
    BoxEmu::get().palette(palette, PALETTE_SIZE);

    // render the frame
    BoxEmu::get().push_frame((uint8_t*)bitmap.data + frame_buffer_offset);
    // ping pong the frame buffer
    frame_buffer_index = !frame_buffer_index;
    bitmap.data = frame_buffer_index
      ? (uint8_t*)BoxEmu::get().frame_buffer1()
      : (uint8_t*)BoxEmu::get().frame_buffer0();
  } else {
    system_frame(1);
  }

  ++frame_counter;

  // Process audio
  for (int x = 0; x < sms_snd->sample_count; x++) {
    uint32_t sample;

    if (muteFrameCount < 60 * 2) {
      // When the emulator starts, audible poping is generated.
      // Audio should be disabled during this startup period.
      sample = 0;
      ++muteFrameCount;
    } else {
      sample = (sms_snd->output[0][x] << 16) + sms_snd->output[1][x];
    }

    sms_audio_buffer[x] = sample;
  }
  auto sms_audio_buffer_len = sms_snd->sample_count - 1;

  // push the audio buffer to the audio task
  BoxEmu::get().play_audio((uint8_t*)sms_audio_buffer, sms_audio_buffer_len * 2 * 2); // 2 channels, 2 bytes per sample

  // update unlock based on x button
  static bool last_x = false;
  if (state.x && !last_x) {
    unlock = !unlock;
  }
  last_x = state.x;

  // manage statistics
  auto end = esp_timer_get_time();
  auto elapsed = end - start;
  update_frame_time(elapsed);
  static constexpr uint64_t max_frame_time = 1000000 / 60;
  if (!unlock && elapsed < max_frame_time) {
    auto sleep_time = (max_frame_time - elapsed) / 1e3;
    std::this_thread::sleep_for(sleep_time * std::chrono::milliseconds(1));
  }
}

void load_sms(std::string_view save_path) {
  if (save_path.size()) {
    auto f = fopen(save_path.data(), "rb");
    system_load_state(f);
    fclose(f);
  }
}

void save_sms(std::string_view save_path) {
  // open the save path as a file descriptor
  auto f = fopen(save_path.data(), "wb");
  system_save_state(f);
  fclose(f);
}

std::span<uint8_t> get_sms_video_buffer() {
  int height = is_gg ? GG_VISIBLE_HEIGHT : SMS_VISIBLE_HEIGHT;
  int width = is_gg ? GG_SCREEN_WIDTH : SMS_SCREEN_WIDTH;
  int pitch = SMS_SCREEN_WIDTH;

  uint8_t *span_ptr = !frame_buffer_index
      ? (uint8_t*)BoxEmu::get().frame_buffer1()
      : (uint8_t*)BoxEmu::get().frame_buffer0();

  std::span<uint8_t> frame(span_ptr, width * height * 2);

  // the frame data for the SMS is stored in bitmap.data as a 8 bit index into
  // the palette we need to convert this to a 16 bit RGB565 value
  const uint8_t *frame_buffer = bitmap.data + frame_buffer_offset;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      uint8_t index = frame_buffer[y * pitch + x];
      uint16_t rgb565 = palette[index % PALETTE_SIZE];
      frame[(y * width + x)*2] = rgb565 & 0xFF;
      frame[(y * width + x)*2+1] = (rgb565 >> 8) & 0xFF;
    }
  }
  return frame;
}

void deinit_sms() {
  shared_mem_clear();
}
