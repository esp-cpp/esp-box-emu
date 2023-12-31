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

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static constexpr size_t SMS_SCREEN_WIDTH = 256;
static constexpr size_t SMS_VISIBLE_HEIGHT = 192;

static std::atomic<bool> scaled = false;
static std::atomic<bool> filled = true;

// NOTE: no longer used
// // rgb888
// const uint32_t sms_palette_rgb[0x100] = {
//     0x00000000,0x00000048,0x000000B4,0x000000FF,0x00002400,0x00002448,0x000024B4,0x000024FF,
//     0x00004800,0x00004848,0x000048B4,0x000048FF,0x00006C00,0x00006C48,0x00006CB4,0x00006CFF,
//     0x00009000,0x00009048,0x000090B4,0x000090FF,0x0000B400,0x0000B448,0x0000B4B4,0x0000B4FF,
//     0x0000D800,0x0000D848,0x0000D8B4,0x0000D8FF,0x0000FF00,0x0000FF48,0x0000FFB4,0x0000FFFF,
//     0x00240000,0x00240048,0x002400B4,0x002400FF,0x00242400,0x00242448,0x002424B4,0x002424FF,
//     0x00244800,0x00244848,0x002448B4,0x002448FF,0x00246C00,0x00246C48,0x00246CB4,0x00246CFF,
//     0x00249000,0x00249048,0x002490B4,0x002490FF,0x0024B400,0x0024B448,0x0024B4B4,0x0024B4FF,
//     0x0024D800,0x0024D848,0x0024D8B4,0x0024D8FF,0x0024FF00,0x0024FF48,0x0024FFB4,0x0024FFFF,
//     0x00480000,0x00480048,0x004800B4,0x004800FF,0x00482400,0x00482448,0x004824B4,0x004824FF,
//     0x00484800,0x00484848,0x004848B4,0x004848FF,0x00486C00,0x00486C48,0x00486CB4,0x00486CFF,
//     0x00489000,0x00489048,0x004890B4,0x004890FF,0x0048B400,0x0048B448,0x0048B4B4,0x0048B4FF,
//     0x0048D800,0x0048D848,0x0048D8B4,0x0048D8FF,0x0048FF00,0x0048FF48,0x0048FFB4,0x0048FFFF,
//     0x006C0000,0x006C0048,0x006C00B4,0x006C00FF,0x006C2400,0x006C2448,0x006C24B4,0x006C24FF,
//     0x006C4800,0x006C4848,0x006C48B4,0x006C48FF,0x006C6C00,0x006C6C48,0x006C6CB4,0x006C6CFF,
//     0x006C9000,0x006C9048,0x006C90B4,0x006C90FF,0x006CB400,0x006CB448,0x006CB4B4,0x006CB4FF,
//     0x006CD800,0x006CD848,0x006CD8B4,0x006CD8FF,0x006CFF00,0x006CFF48,0x006CFFB4,0x006CFFFF,
//     0x00900000,0x00900048,0x009000B4,0x009000FF,0x00902400,0x00902448,0x009024B4,0x009024FF,
//     0x00904800,0x00904848,0x009048B4,0x009048FF,0x00906C00,0x00906C48,0x00906CB4,0x00906CFF,
//     0x00909000,0x00909048,0x009090B4,0x009090FF,0x0090B400,0x0090B448,0x0090B4B4,0x0090B4FF,
//     0x0090D800,0x0090D848,0x0090D8B4,0x0090D8FF,0x0090FF00,0x0090FF48,0x0090FFB4,0x0090FFFF,
//     0x00B40000,0x00B40048,0x00B400B4,0x00B400FF,0x00B42400,0x00B42448,0x00B424B4,0x00B424FF,
//     0x00B44800,0x00B44848,0x00B448B4,0x00B448FF,0x00B46C00,0x00B46C48,0x00B46CB4,0x00B46CFF,
//     0x00B49000,0x00B49048,0x00B490B4,0x00B490FF,0x00B4B400,0x00B4B448,0x00B4B4B4,0x00B4B4FF,
//     0x00B4D800,0x00B4D848,0x00B4D8B4,0x00B4D8FF,0x00B4FF00,0x00B4FF48,0x00B4FFB4,0x00B4FFFF,
//     0x00D80000,0x00D80048,0x00D800B4,0x00D800FF,0x00D82400,0x00D82448,0x00D824B4,0x00D824FF,
//     0x00D84800,0x00D84848,0x00D848B4,0x00D848FF,0x00D86C00,0x00D86C48,0x00D86CB4,0x00D86CFF,
//     0x00D89000,0x00D89048,0x00D890B4,0x00D890FF,0x00D8B400,0x00D8B448,0x00D8B4B4,0x00D8B4FF,
//     0x00D8D800,0x00D8D848,0x00D8D8B4,0x00D8D8FF,0x00D8FF00,0x00D8FF48,0x00D8FFB4,0x00D8FFFF,
//     0x00FF0000,0x00FF0048,0x00FF00B4,0x00FF00FF,0x00FF2400,0x00FF2448,0x00FF24B4,0x00FF24FF,
//     0x00FF4800,0x00FF4848,0x00FF48B4,0x00FF48FF,0x00FF6C00,0x00FF6C48,0x00FF6CB4,0x00FF6CFF,
//     0x00FF9000,0x00FF9048,0x00FF90B4,0x00FF90FF,0x00FFB400,0x00FFB448,0x00FFB4B4,0x00FFB4FF,
//     0x00FFD800,0x00FFD848,0x00FFD8B4,0x00FFD8FF,0x00FFFF00,0x00FFFF48,0x00FFFFB4,0x00FFFFFF,
// };

// a converted version of the rgb888 palette to rgb565
const uint16_t sms_palette_rgb565_inv[0x100] = {
0, 18432, 45056, 63488, 288, 18720, 45344, 63776, 576, 19008, 45632, 64064, 864, 19296, 45920, 64352, 1152, 19584, 46208, 64640, 1440, 19872, 46496, 64928, 1728, 20160, 46784, 65216, 2016, 20448, 47072, 65504, 4, 18436, 45060, 63492, 292, 18724, 45348, 63780, 580, 19012, 45636, 64068, 868, 19300, 45924, 64356, 1156, 19588, 46212, 64644, 1444, 19876, 46500, 64932, 1732, 20164, 46788, 65220, 2020, 20452, 47076, 65508, 9, 18441, 45065, 63497, 297, 18729, 45353, 63785, 585, 19017, 45641, 64073, 873, 19305, 45929, 64361, 1161, 19593, 46217, 64649, 1449, 19881, 46505, 64937, 1737, 20169, 46793, 65225, 2025, 20457, 47081, 65513, 13, 18445, 45069, 63501, 301, 18733, 45357, 63789, 589, 19021, 45645, 64077, 877, 19309, 45933, 64365, 1165, 19597, 46221, 64653, 1453, 19885, 46509, 64941, 1741, 20173, 46797, 65229, 2029, 20461, 47085, 65517, 18, 18450, 45074, 63506, 306, 18738, 45362, 63794, 594, 19026, 45650, 64082, 882, 19314, 45938, 64370, 1170, 19602, 46226, 64658, 1458, 19890, 46514, 64946, 1746, 20178, 46802, 65234, 2034, 20466, 47090, 65522, 22, 18454, 45078, 63510, 310, 18742, 45366, 63798, 598, 19030, 45654, 64086, 886, 19318, 45942, 64374, 1174, 19606, 46230, 64662, 1462, 19894, 46518, 64950, 1750, 20182, 46806, 65238, 2038, 20470, 47094, 65526, 27, 18459, 45083, 63515, 315, 18747, 45371, 63803, 603, 19035, 45659, 64091, 891, 19323, 45947, 64379, 1179, 19611, 46235, 64667, 1467, 19899, 46523, 64955, 1755, 20187, 46811, 65243, 2043, 20475, 47099, 65531, 31, 18463, 45087, 63519, 319, 18751, 45375, 63807, 607, 19039, 45663, 64095, 895, 19327, 45951, 64383, 1183, 19615, 46239, 64671, 1471, 19903, 46527, 64959, 1759, 20191, 46815, 65247, 2047, 20479, 47103, 65535
// 0, 9, 22, 31, 288, 297, 310, 319, 576, 585, 598, 607, 864, 873, 886, 895, 1152, 1161, 1174, 1183, 1440, 1449, 1462, 1471, 1728, 1737, 1750, 1759, 2016, 2025, 2038, 2047, 8192, 8201, 8214, 8223, 8480, 8489, 8502, 8511, 8768, 8777, 8790, 8799, 9056, 9065, 9078, 9087, 9344, 9353, 9366, 9375, 9632, 9641, 9654, 9663, 9920, 9929, 9942, 9951, 10208, 10217, 10230, 10239, 18432, 18441, 18454, 18463, 18720, 18729, 18742, 18751, 19008, 19017, 19030, 19039, 19296, 19305, 19318, 19327, 19584, 19593, 19606, 19615, 19872, 19881, 19894, 19903, 20160, 20169, 20182, 20191, 20448, 20457, 20470, 20479, 26624, 26633, 26646, 26655, 26912, 26921, 26934, 26943, 27200, 27209, 27222, 27231, 27488, 27497, 27510, 27519, 27776, 27785, 27798, 27807, 28064, 28073, 28086, 28095, 28352, 28361, 28374, 28383, 28640, 28649, 28662, 28671, 36864, 36873, 36886, 36895, 37152, 37161, 37174, 37183, 37440, 37449, 37462, 37471, 37728, 37737, 37750, 37759, 38016, 38025, 38038, 38047, 38304, 38313, 38326, 38335, 38592, 38601, 38614, 38623, 38880, 38889, 38902, 38911, 45056, 45065, 45078, 45087, 45344, 45353, 45366, 45375, 45632, 45641, 45654, 45663, 45920, 45929, 45942, 45951, 46208, 46217, 46230, 46239, 46496, 46505, 46518, 46527, 46784, 46793, 46806, 46815, 47072, 47081, 47094, 47103, 55296, 55305, 55318, 55327, 55584, 55593, 55606, 55615, 55872, 55881, 55894, 55903, 56160, 56169, 56182, 56191, 56448, 56457, 56470, 56479, 56736, 56745, 56758, 56767, 57024, 57033, 57046, 57055, 57312, 57321, 57334, 57343, 63488, 63497, 63510, 63519, 63776, 63785, 63798, 63807, 64064, 64073, 64086, 64095, 64352, 64361, 64374, 64383, 64640, 64649, 64662, 64671, 64928, 64937, 64950, 64959, 65216, 65225, 65238, 65247, 65504, 65513, 65526, 65535
};

const uint16_t sms_palette_rgb565[0x100] = {
// 0, 18432, 45056, 63488, 288, 18720, 45344, 63776, 576, 19008, 45632, 64064, 864, 19296, 45920, 64352, 1152, 19584, 46208, 64640, 1440, 19872, 46496, 64928, 1728, 20160, 46784, 65216, 2016, 20448, 47072, 65504, 4, 18436, 45060, 63492, 292, 18724, 45348, 63780, 580, 19012, 45636, 64068, 868, 19300, 45924, 64356, 1156, 19588, 46212, 64644, 1444, 19876, 46500, 64932, 1732, 20164, 46788, 65220, 2020, 20452, 47076, 65508, 9, 18441, 45065, 63497, 297, 18729, 45353, 63785, 585, 19017, 45641, 64073, 873, 19305, 45929, 64361, 1161, 19593, 46217, 64649, 1449, 19881, 46505, 64937, 1737, 20169, 46793, 65225, 2025, 20457, 47081, 65513, 13, 18445, 45069, 63501, 301, 18733, 45357, 63789, 589, 19021, 45645, 64077, 877, 19309, 45933, 64365, 1165, 19597, 46221, 64653, 1453, 19885, 46509, 64941, 1741, 20173, 46797, 65229, 2029, 20461, 47085, 65517, 18, 18450, 45074, 63506, 306, 18738, 45362, 63794, 594, 19026, 45650, 64082, 882, 19314, 45938, 64370, 1170, 19602, 46226, 64658, 1458, 19890, 46514, 64946, 1746, 20178, 46802, 65234, 2034, 20466, 47090, 65522, 22, 18454, 45078, 63510, 310, 18742, 45366, 63798, 598, 19030, 45654, 64086, 886, 19318, 45942, 64374, 1174, 19606, 46230, 64662, 1462, 19894, 46518, 64950, 1750, 20182, 46806, 65238, 2038, 20470, 47094, 65526, 27, 18459, 45083, 63515, 315, 18747, 45371, 63803, 603, 19035, 45659, 64091, 891, 19323, 45947, 64379, 1179, 19611, 46235, 64667, 1467, 19899, 46523, 64955, 1755, 20187, 46811, 65243, 2043, 20475, 47099, 65531, 31, 18463, 45087, 63519, 319, 18751, 45375, 63807, 607, 19039, 45663, 64095, 895, 19327, 45951, 64383, 1183, 19615, 46239, 64671, 1471, 19903, 46527, 64959, 1759, 20191, 46815, 65247, 2047, 20479, 47103, 65535
0, 9, 22, 31, 288, 297, 310, 319, 576, 585, 598, 607, 864, 873, 886, 895, 1152, 1161, 1174, 1183, 1440, 1449, 1462, 1471, 1728, 1737, 1750, 1759, 2016, 2025, 2038, 2047, 8192, 8201, 8214, 8223, 8480, 8489, 8502, 8511, 8768, 8777, 8790, 8799, 9056, 9065, 9078, 9087, 9344, 9353, 9366, 9375, 9632, 9641, 9654, 9663, 9920, 9929, 9942, 9951, 10208, 10217, 10230, 10239, 18432, 18441, 18454, 18463, 18720, 18729, 18742, 18751, 19008, 19017, 19030, 19039, 19296, 19305, 19318, 19327, 19584, 19593, 19606, 19615, 19872, 19881, 19894, 19903, 20160, 20169, 20182, 20191, 20448, 20457, 20470, 20479, 26624, 26633, 26646, 26655, 26912, 26921, 26934, 26943, 27200, 27209, 27222, 27231, 27488, 27497, 27510, 27519, 27776, 27785, 27798, 27807, 28064, 28073, 28086, 28095, 28352, 28361, 28374, 28383, 28640, 28649, 28662, 28671, 36864, 36873, 36886, 36895, 37152, 37161, 37174, 37183, 37440, 37449, 37462, 37471, 37728, 37737, 37750, 37759, 38016, 38025, 38038, 38047, 38304, 38313, 38326, 38335, 38592, 38601, 38614, 38623, 38880, 38889, 38902, 38911, 45056, 45065, 45078, 45087, 45344, 45353, 45366, 45375, 45632, 45641, 45654, 45663, 45920, 45929, 45942, 45951, 46208, 46217, 46230, 46239, 46496, 46505, 46518, 46527, 46784, 46793, 46806, 46815, 47072, 47081, 47094, 47103, 55296, 55305, 55318, 55327, 55584, 55593, 55606, 55615, 55872, 55881, 55894, 55903, 56160, 56169, 56182, 56191, 56448, 56457, 56470, 56479, 56736, 56745, 56758, 56767, 57024, 57033, 57046, 57055, 57312, 57321, 57334, 57343, 63488, 63497, 63510, 63519, 63776, 63785, 63798, 63807, 64064, 64073, 64086, 64095, 64352, 64361, 64374, 64383, 64640, 64649, 64662, 64671, 64928, 64937, 64950, 64959, 65216, 65225, 65238, 65247, 65504, 65513, 65526, 65535
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
  }

  // set native size
  hal::set_native_size(SMS_SCREEN_WIDTH, SMS_VISIBLE_HEIGHT);
  hal::set_palette(sms_palette_rgb565_inv);

  reset_sms();

  memset(&sms_snd, 0, sizeof(t_sms_snd));
  sms_snd.buffer[0] = (int16_t*)get_audio_buffer();
  sms_snd.buffer[1] = (int16_t*)get_audio_buffer() + 262;

  size_t audio_frequency = 15720; // 15600;
  // size_t audio_frequency = 0;
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

  hal::push_frame(bitmap.data);
  // TODO: ping pong the frame buffer

  // TODO: push the audio buffer to the audio task

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
    uint16_t rgb565 = sms_palette_rgb565[index];
    frame[i*2] = (rgb565 >> 8) & 0xFF;
    frame[i*2+1] = rgb565 & 0xFF;
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
