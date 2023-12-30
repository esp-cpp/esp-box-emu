#include "sms.hpp"

extern "C" {
#include "smsplus/shared.h"
#include "smsplus/system.h"
#include "smsplus/sms.h"
};

#include <string>

#include "format.hpp"
#include "fs_init.hpp"
#include "i2s_audio.h"
#include "input.h"
#include "spi_lcd.h"
#include "st7789.hpp"
#include "task.hpp"
#include "statistics.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static const size_t SCREEN_WIDTH = 320;
static const size_t SCREEN_HEIGHT = 240;
static constexpr size_t SMS_SCREEN_WIDTH = 256;
static constexpr size_t SMS_VISIBLE_HEIGHT = 192;

static std::atomic<bool> scaled = false;
static std::atomic<bool> filled = true;

// used by sim, rgb332 to rgb888
const uint32_t sms_palette_rgb[0x100] = {
    0x00000000,0x00000048,0x000000B4,0x000000FF,0x00002400,0x00002448,0x000024B4,0x000024FF,
    0x00004800,0x00004848,0x000048B4,0x000048FF,0x00006C00,0x00006C48,0x00006CB4,0x00006CFF,
    0x00009000,0x00009048,0x000090B4,0x000090FF,0x0000B400,0x0000B448,0x0000B4B4,0x0000B4FF,
    0x0000D800,0x0000D848,0x0000D8B4,0x0000D8FF,0x0000FF00,0x0000FF48,0x0000FFB4,0x0000FFFF,
    0x00240000,0x00240048,0x002400B4,0x002400FF,0x00242400,0x00242448,0x002424B4,0x002424FF,
    0x00244800,0x00244848,0x002448B4,0x002448FF,0x00246C00,0x00246C48,0x00246CB4,0x00246CFF,
    0x00249000,0x00249048,0x002490B4,0x002490FF,0x0024B400,0x0024B448,0x0024B4B4,0x0024B4FF,
    0x0024D800,0x0024D848,0x0024D8B4,0x0024D8FF,0x0024FF00,0x0024FF48,0x0024FFB4,0x0024FFFF,
    0x00480000,0x00480048,0x004800B4,0x004800FF,0x00482400,0x00482448,0x004824B4,0x004824FF,
    0x00484800,0x00484848,0x004848B4,0x004848FF,0x00486C00,0x00486C48,0x00486CB4,0x00486CFF,
    0x00489000,0x00489048,0x004890B4,0x004890FF,0x0048B400,0x0048B448,0x0048B4B4,0x0048B4FF,
    0x0048D800,0x0048D848,0x0048D8B4,0x0048D8FF,0x0048FF00,0x0048FF48,0x0048FFB4,0x0048FFFF,
    0x006C0000,0x006C0048,0x006C00B4,0x006C00FF,0x006C2400,0x006C2448,0x006C24B4,0x006C24FF,
    0x006C4800,0x006C4848,0x006C48B4,0x006C48FF,0x006C6C00,0x006C6C48,0x006C6CB4,0x006C6CFF,
    0x006C9000,0x006C9048,0x006C90B4,0x006C90FF,0x006CB400,0x006CB448,0x006CB4B4,0x006CB4FF,
    0x006CD800,0x006CD848,0x006CD8B4,0x006CD8FF,0x006CFF00,0x006CFF48,0x006CFFB4,0x006CFFFF,
    0x00900000,0x00900048,0x009000B4,0x009000FF,0x00902400,0x00902448,0x009024B4,0x009024FF,
    0x00904800,0x00904848,0x009048B4,0x009048FF,0x00906C00,0x00906C48,0x00906CB4,0x00906CFF,
    0x00909000,0x00909048,0x009090B4,0x009090FF,0x0090B400,0x0090B448,0x0090B4B4,0x0090B4FF,
    0x0090D800,0x0090D848,0x0090D8B4,0x0090D8FF,0x0090FF00,0x0090FF48,0x0090FFB4,0x0090FFFF,
    0x00B40000,0x00B40048,0x00B400B4,0x00B400FF,0x00B42400,0x00B42448,0x00B424B4,0x00B424FF,
    0x00B44800,0x00B44848,0x00B448B4,0x00B448FF,0x00B46C00,0x00B46C48,0x00B46CB4,0x00B46CFF,
    0x00B49000,0x00B49048,0x00B490B4,0x00B490FF,0x00B4B400,0x00B4B448,0x00B4B4B4,0x00B4B4FF,
    0x00B4D800,0x00B4D848,0x00B4D8B4,0x00B4D8FF,0x00B4FF00,0x00B4FF48,0x00B4FFB4,0x00B4FFFF,
    0x00D80000,0x00D80048,0x00D800B4,0x00D800FF,0x00D82400,0x00D82448,0x00D824B4,0x00D824FF,
    0x00D84800,0x00D84848,0x00D848B4,0x00D848FF,0x00D86C00,0x00D86C48,0x00D86CB4,0x00D86CFF,
    0x00D89000,0x00D89048,0x00D890B4,0x00D890FF,0x00D8B400,0x00D8B448,0x00D8B4B4,0x00D8B4FF,
    0x00D8D800,0x00D8D848,0x00D8D8B4,0x00D8D8FF,0x00D8FF00,0x00D8FF48,0x00D8FFB4,0x00D8FFFF,
    0x00FF0000,0x00FF0048,0x00FF00B4,0x00FF00FF,0x00FF2400,0x00FF2448,0x00FF24B4,0x00FF24FF,
    0x00FF4800,0x00FF4848,0x00FF48B4,0x00FF48FF,0x00FF6C00,0x00FF6C48,0x00FF6CB4,0x00FF6CFF,
    0x00FF9000,0x00FF9048,0x00FF90B4,0x00FF90FF,0x00FFB400,0x00FFB448,0x00FFB4B4,0x00FFB4FF,
    0x00FFD800,0x00FFD848,0x00FFD8B4,0x00FFD8FF,0x00FFFF00,0x00FFFF48,0x00FFFFB4,0x00FFFFFF,
};

void sms_frame(int skip_render);
void sms_init(void);
void sms_reset(void);

// big mem req
void set_sms_video_original() {
  scaled = false;
  filled = false;
  // TODO:
}

void set_sms_video_fit() {
  scaled = true;
  filled = false;
  // TODO:
}

void set_sms_video_fill() {
  scaled = false;
  filled = true;
  // TODO:
}

void reset_sms() {
  system_reset();
}

// TODO: make an audio play task

// AUDIO:

    // static int16_t S16(int i)
    // {
    //     return (i << 1) ^ 0x8000;
    // }

    // int _lp;
    // virtual int audio_buffer(int16_t* b, int len)
    // {
    //     int n = frame_sample_count();
    //     for (int i = 0; i < n; i++) {
    //         int s = (sms_snd.buffer[0][i] + sms_snd.buffer[1][i]) >> 1;
    //         _lp = (_lp*31 + s) >> 5;    // lo pass
    //         s -= _lp;                   // signed
    //         if (s < -32767) s = -32767; // clip
    //         if (s > 32767) s = 32767;
    //         *b++ = s;                   // centered signed 1 channel
    //     }
    //     return n;
    // }

// TODO: make a video render task using the frame buffer and the palette

static void init(uint8_t *romdata, size_t rom_data_size) {
  static bool initialized = false;
  cart.pages = ((rom_data_size + 0x3FFF)/0x4000);
  cart.rom = romdata;
  if (!initialized) {
    auto sms_videodata = get_frame_buffer0(); // only needs 256x240
    bitmap.data = sms_videodata + 24*256;
    bitmap.width = 256;
    bitmap.height = 192;
    bitmap.pitch = 256;
    bitmap.depth = 8;
    sms.dummy = sms_videodata;
    sms.sram = (uint8_t*)heap_caps_malloc(0x8000, MALLOC_CAP_SPIRAM);
    sms.ram = (uint8_t*)heap_caps_malloc(0x2000, MALLOC_CAP_SPIRAM);
    memset(&sms_snd, 0, sizeof(t_sms_snd));
    sms_snd.buffer[0] = (int16_t*)get_audio_buffer();
    sms_snd.buffer[1] = (int16_t*)get_audio_buffer() + 262;
  }

  size_t audio_frequency = 15720; // 15600;
  emu_system_init(audio_frequency);
  sms_init();

  initialized = true;
  reset_frame_time();
}

void init_sms(uint8_t *romdata, size_t rom_data_size) {
  cart.type = TYPE_SMS;
  init(romdata, rom_data_size);
  fmt::print("sms init done\n");
}

void init_gg(uint8_t *romdata, size_t rom_data_size) {
  cart.type = TYPE_GG;
  init(romdata, rom_data_size);
  fmt::print("gg init done\n");
}

void run_sms_rom() {
  auto start = std::chrono::high_resolution_clock::now();
  // handle input here (see system.h and use input.pad and input.system)
  InputState state;
  get_input_state(&state);

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

  input.system = 0;

  // emulate the frame
  bool skip_frame_rendering = false;
  sms_frame(skip_frame_rendering);

  // temporary: render the frame buffer to the screen
  static constexpr int num_lines_to_write = NUM_ROWS_IN_FRAME_BUFFER;
  static int vram_index = 0;
  auto _frame = bitmap.data;
  constexpr int x_offset = (SCREEN_WIDTH-SMS_SCREEN_WIDTH)/2;
  constexpr int y_offset = (SCREEN_HEIGHT-SMS_VISIBLE_HEIGHT)/2;
  for (int y=0; y<SMS_VISIBLE_HEIGHT; y+= num_lines_to_write) {
    uint16_t* _buf = vram_index ? (uint16_t*)get_vram1() : (uint16_t*)get_vram0();
    vram_index = vram_index ? 0 : 1;
    int num_lines = std::min<int>(num_lines_to_write, SMS_VISIBLE_HEIGHT-y);
    for (int i = 0; i < SMS_SCREEN_WIDTH * num_lines; i++) {
      uint8_t index = _frame[y*SMS_SCREEN_WIDTH+i];
      uint16_t color = sms_palette_rgb[index];
      // break out the RGB888 color into RGB565
      uint8_t r = (color >> 16) & 0xFF;
      uint8_t g = (color >> 8) & 0xFF;
      uint8_t b = color & 0xFF;
      _buf[i] = make_color(r, g, b);
    }
    // memcpy(_buf, &_frame[y*SMS_SCREEN_WIDTH], num_lines*SMS_SCREEN_WIDTH*2);
    lcd_write_frame(x_offset, y + y_offset, SMS_SCREEN_WIDTH, num_lines, (uint8_t*)&_buf[0]);
  }

  // manage statistics
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration<float>(end-start).count();
  update_frame_time(elapsed);
  // frame rate should be 60 FPS, so 1/60th second is what we want to sleep for
  static constexpr auto delay = std::chrono::duration<float>(1.0f/60.0f);
  std::this_thread::sleep_until(start + delay);
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

std::vector<uint8_t> get_sms_video_buffer() {
  std::vector<uint8_t> frame(SMS_SCREEN_WIDTH * SMS_VISIBLE_HEIGHT * 2);
  // the frame data for the SMS is stored in bitmap.data as a 8 bit index into
  // the palette we need to convert this to a 16 bit RGB565 value
  const uint8_t *frame_buffer = bitmap.data;
  for (int i = 0; i < SMS_SCREEN_WIDTH * SMS_VISIBLE_HEIGHT; i++) {
    uint8_t index = frame_buffer[i];
    uint16_t color = sms_palette_rgb[index];
    // break out the RGB888 color into RGB565
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    uint16_t rgb565 = make_color(r, g, b);
    frame[i*2] = rgb565 & 0xFF;
    frame[i*2+1] = (rgb565 >> 8) & 0xFF;
  }
  return frame;
}

void stop_sms_tasks() {
  // TODO:
}

void start_sms_tasks() {
  // TODO:
}

void deinit_sms() {
  // TODO:
}
