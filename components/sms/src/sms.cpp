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
#include "video_task.hpp"
#include "audio_task.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static constexpr size_t SMS_SCREEN_WIDTH = 256;
static constexpr size_t SMS_VISIBLE_HEIGHT = 192;

static constexpr size_t GG_SCREEN_WIDTH = 160;
static constexpr size_t GG_VISIBLE_HEIGHT = 144;

// a converted version of the rgb888 palette to rgb565
static const uint16_t sms_palette_rgb565[0x100] = {
0, 2304, 5632, 7936, 8193, 10497, 13825, 16129, 16386, 18690, 22018, 24322, 24579, 26883, 30211, 32515, 32772, 35076, 38404, 40708, 40965, 43269, 46597, 48901, 49158, 51462, 54790, 57094, 57351, 59655, 62983, 65287, 32, 2336, 5664, 7968, 8225, 10529, 13857, 16161, 16418, 18722, 22050, 24354, 24611, 26915, 30243, 32547, 32804, 35108, 38436, 40740, 40997, 43301, 46629, 48933, 49190, 51494, 54822, 57126, 57383, 59687, 63015, 65319, 72, 2376, 5704, 8008, 8265, 10569, 13897, 16201, 16458, 18762, 22090, 24394, 24651, 26955, 30283, 32587, 32844, 35148, 38476, 40780, 41037, 43341, 46669, 48973, 49230, 51534, 54862, 57166, 57423, 59727, 63055, 65359, 104, 2408, 5736, 8040, 8297, 10601, 13929, 16233, 16490, 18794, 22122, 24426, 24683, 26987, 30315, 32619, 32876, 35180, 38508, 40812, 41069, 43373, 46701, 49005, 49262, 51566, 54894, 57198, 57455, 59759, 63087, 65391, 144, 2448, 5776, 8080, 8337, 10641, 13969, 16273, 16530, 18834, 22162, 24466, 24723, 27027, 30355, 32659, 32916, 35220, 38548, 40852, 41109, 43413, 46741, 49045, 49302, 51606, 54934, 57238, 57495, 59799, 63127, 65431, 176, 2480, 5808, 8112, 8369, 10673, 14001, 16305, 16562, 18866, 22194, 24498, 24755, 27059, 30387, 32691, 32948, 35252, 38580, 40884, 41141, 43445, 46773, 49077, 49334, 51638, 54966, 57270, 57527, 59831, 63159, 65463, 216, 2520, 5848, 8152, 8409, 10713, 14041, 16345, 16602, 18906, 22234, 24538, 24795, 27099, 30427, 32731, 32988, 35292, 38620, 40924, 41181, 43485, 46813, 49117, 49374, 51678, 55006, 57310, 57567, 59871, 63199, 65503, 248, 2552, 5880, 8184, 8441, 10745, 14073, 16377, 16634, 18938, 22266, 24570, 24827, 27131, 30459, 32763, 33020, 35324, 38652, 40956, 41213, 43517, 46845, 49149, 49406, 51710, 55038, 57342, 57599, 59903, 63231, 65535
};

void sms_frame(int skip_render);
void sms_init(void);
void sms_reset(void);

void reset_sms() {
  system_reset();
}

static size_t audio_frequency = 16000; // 15720;
static int32_t _lp{0};
static int32_t _lp_left{0};
static int32_t _lp_right{0};
static int low_pass_audio(int32_t sample, int32_t &lp) {
  lp = (lp*31 + sample) >> 5;    // lo pass
  sample -= lp;                   // signed
  if (sample < -32767) sample = -32767; // clip
  if (sample > 32767) sample = 32767;
  return sample;
}

static int audio_buffer(int16_t* b, int len) {
  int n = AUDIO_SAMPLE_COUNT;
  for (int i = 0; i < n; i++) {
    // // copy the audio data from each of the two sms_snd.buffers into a single buffer
    // *b++ = sms_snd.buffer[0][i];
    // *b++ = sms_snd.buffer[1][i];

    // mix the audio data from each of the two sms_snd.buffers into a single buffer
    *b++ = low_pass_audio(sms_snd.buffer[0][i], _lp_left);
    *b++ = low_pass_audio(sms_snd.buffer[1][i], _lp_right);

    // // original:
    // int s = (sms_snd.buffer[0][i] + sms_snd.buffer[1][i]) >> 1;
    // _lp = (_lp*31 + s) >> 5;    // lo pass
    // s -= _lp;                   // signed
    // if (s < -32767) s = -32767; // clip
    // if (s > 32767) s = 32767;
    // *b++ = s;                   // centered signed 1 channel
  }
  return n;
}

static uint8_t *sms_dummy = nullptr;
static uint8_t *sms_sram = nullptr;
static uint8_t *sms_ram = nullptr;

static int frame_buffer_index = 0;
static void init(uint8_t *romdata, size_t rom_data_size) {
  static bool initialized = false;
  if (!initialized) {
    sms_sram = (uint8_t*)heap_caps_malloc(0x8000, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    sms_ram = (uint8_t*)heap_caps_malloc(0x2000, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    sms_dummy = (uint8_t*)heap_caps_malloc(0x2000, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  }

  bitmap.width = SMS_SCREEN_WIDTH;
  bitmap.height = SMS_VISIBLE_HEIGHT;
  bitmap.pitch = SMS_SCREEN_WIDTH;

  // set native size
  hal::set_palette(sms_palette_rgb565);
  hal::set_native_size(bitmap.width, bitmap.height);

  cart.pages = ((rom_data_size + 0x3FFF)/0x4000);
  cart.rom = romdata;

  bitmap.data = get_frame_buffer0();
  bitmap.depth = 8;

  sms.dummy = sms_dummy;
  sms.sram = sms_sram;
  sms.ram = sms_ram;

  emu_system_init(audio_frequency);
  sms_init();

  initialized = true;
  reset_frame_time();
}

void init_sms(uint8_t *romdata, size_t rom_data_size) {
  reset_sms();

  cart.type = TYPE_SMS;
  init(romdata, rom_data_size);
  fmt::print("sms init done\n");
}

void init_gg(uint8_t *romdata, size_t rom_data_size) {
  reset_sms();

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

  // system is where we input the start button, as well as soft reset
  input.system = 0;
  input.system |= state.start ? INPUT_START : 0;
  input.system |= state.select ? INPUT_PAUSE : 0;

  // emulate the frame
  memset(bitmap.data, 0, 256*192);
  bool skip_frame_rendering = false;
  sms_frame(skip_frame_rendering);

  hal::push_frame(bitmap.data);
  // ping pong the frame buffer
  frame_buffer_index = !frame_buffer_index;
  bitmap.data = frame_buffer_index ? (uint8_t*)get_frame_buffer1() : (uint8_t*)get_frame_buffer0();

  // copy the audio data into one of the dma-ed audio buffers
  static int audio_buffer_index = 0;
  int16_t *audio_buffer_ptr = audio_buffer_index ? (int16_t*)get_audio_buffer1() : (int16_t*)get_audio_buffer0();
  audio_buffer_index = !audio_buffer_index;
  int audio_buffer_len = audio_buffer(audio_buffer_ptr, AUDIO_BUFFER_SIZE);

  // push the audio buffer to the audio task
  hal::set_audio_sample_count(audio_buffer_len);
  hal::push_audio(audio_buffer_ptr);

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
  int height = bitmap.height;
  int width = bitmap.width;
  std::vector<uint8_t> frame(width * height * 2);
  // the frame data for the SMS is stored in bitmap.data as a 8 bit index into
  // the palette we need to convert this to a 16 bit RGB565 value
  const uint8_t *frame_buffer = bitmap.data;
  for (int i = 0; i < width * height; i++) {
    uint8_t index = frame_buffer[i];
    uint16_t rgb565 = sms_palette_rgb565[index];
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
