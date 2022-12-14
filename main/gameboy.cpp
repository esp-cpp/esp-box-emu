#pragma GCC optimize ("Ofast")

#include "gameboy.hpp"

#include <memory>

#include "fs_init.hpp"

#include "format.hpp"
#include "spi_lcd.h"
#include "i2s_audio.h"
#include "input.h"
#include "st7789.hpp"
#include "task.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static const size_t gameboy_screen_width = 160;
static const size_t gameboy_screen_height = 160;

#if USE_GAMEBOY_GNUBOY
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
uint16_t* displayBuffer[2];
uint16_t* framebuffer;
struct fb fb;
struct pcm pcm;
uint8_t currentBuffer = 0;
int frame = 0;

int32_t* audioBuffer[2];
volatile uint8_t currentAudioBuffer = 0;
volatile uint16_t currentAudioSampleCount;
volatile int32_t* currentAudioBufferPtr;

extern "C" void die(char *fmt, ...) {
  // do nothing...
}

static std::shared_ptr<espp::Task> gbc_task;
static std::shared_ptr<espp::Task> gbc_video_task;
static QueueHandle_t video_queue;
static float totalElapsedSeconds = 0;
static struct InputState state;

static std::atomic<bool> scaled = false;
static std::atomic<bool> filled = false;
void IRAM_ATTR video_task(std::mutex &m, std::condition_variable& cv) {
  static uint16_t *_frame;
  xQueuePeek(video_queue, &_frame, portMAX_DELAY);

  // need to determine if scaling changed, so we can adjust the offsets and clear
  // the screen.
  static bool local_scaled = scaled;
  static bool local_filled = filled;
  if (scaled != local_scaled || filled != local_filled) {
    // adjust our local variables
    local_scaled = scaled;
    local_filled = filled;
    // clear the screen
    lcd_write_frame(0,0,320,240,nullptr);
  }

  if (scaled || filled) {
    static int vram_index = 0;
    int x_offset = filled ? 0 : (320-266)/2;
    int y_offset = 0;
    float y_scale = 1.667f;
    float x_scale = scaled ? y_scale : 2.0f;
    int max_y = (int)(y_scale * 144.0f);
    int max_x = (int)(x_scale * 160.0f);

    static constexpr int num_lines_to_write = 40;
    for (int y=0; y<max_y; y+=num_lines_to_write) {
      uint16_t* _buf = vram_index ? (uint16_t*)get_vram1() : (uint16_t*)get_vram0();
      vram_index = vram_index ? 0 : 1;
      for (int i=0; i<num_lines_to_write; i++) {
        int _y = y+i;
        int source_y = (float)_y/y_scale;
        for (int x=0; x<max_x; x++) {
          int source_x = (float)x/x_scale;
          _buf[i*max_x + x] = _frame[source_y*160 + source_x];
        }
      }
      lcd_write_frame(0 + x_offset, y, max_x, num_lines_to_write, (uint8_t*)&_buf[0]);
    }
  } else {
    // seems like the fastest we can do is 1/2 the screen at a time...
    constexpr int x_offset = (320-160)/2;
    constexpr int y_offset = (240-144)/2;
    static constexpr int num_lines_to_write = 144 / 2;
    for (int y=0; y<144; y+= num_lines_to_write) {
      lcd_write_frame(x_offset, y + y_offset, 160, num_lines_to_write, (uint8_t*)&_frame[y*160]);
    }
  }
  xQueueReceive(video_queue, &_frame, portMAX_DELAY);
}

void IRAM_ATTR run_to_vblank(std::mutex &m, std::condition_variable& cv) {
  /* FRAME BEGIN */
  auto start = std::chrono::high_resolution_clock::now();

  /* FIXME: judging by the time specified this was intended
  to emulate through vblank phase which is handled at the
  end of the loop. */
  cpu_emulate(2280);
  // cpu_emulate(2280 / 2);

  /* FIXME: R_LY >= 0; comparsion to zero can also be removed
  altogether, R_LY is always 0 at this point */
  while (R_LY > 0 && R_LY < 144)
  {
    emu_step();
  }

  /* VBLANK BEGIN */
  if ((frame % 2) == 0) {
    // for (int y=0; y<144; y+=48) {
    //   lcd_write_frame(0, y, 160, 48, (uint8_t*)&framebuffer[y*160]);
    // }
    xQueueSend(video_queue, (void*)&framebuffer, portMAX_DELAY);

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

    audio_play_frame((uint8_t*)currentAudioBufferPtr, currentAudioSampleCount * 2);

    // Swap buffers
    // currentAudioBuffer = currentAudioBuffer ? 0 : 1;
    // pcm.buf = (int16_t*)audioBuffer[currentAudioBuffer];
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
  totalElapsedSeconds += elapsed;
  if ((frame % 60) == 0) {
    fmt::print("gameboy: FPS {}\n", (float) frame / totalElapsedSeconds);
  }
  // frame rate should be 60 FPS, so 1/60th second is what we want to sleep for
  static constexpr auto delay = std::chrono::duration<float>(1.0f/60.0f);
  std::this_thread::sleep_until(start + delay);
}
#endif

void set_gb_video_original() {
  scaled = false;
  filled = false;
}

void set_gb_video_fit() {
  scaled = true;
  filled = false;
}

void set_gb_video_fill() {
  scaled = false;
  filled = true;
}

static std::string gbc_savedir = "/sdcard";
static std::string current_cart = "";

static std::string get_save_path(bool bypass_exist_check=false) {
  namespace fs = std::filesystem;
  fmt::print("creating: {}\n", gbc_savedir);
  // fs::create_directories(gbc_savedir);
  mkdirp(gbc_savedir.c_str());
  auto save_path = gbc_savedir + "/" + fs::path(current_cart).stem().string() + "_gbc.sav";
  if (bypass_exist_check || fs::exists(save_path)) {
    fmt::print("found: {}\n", save_path);
    return save_path;
  } else {
    fmt::print("Could not find {}\n", save_path);
  }
  return "";
}

void init_gameboy(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size) {
  static bool initialized = false;

  current_cart = rom_filename;

  // lcd_set_queued_transmit();
#if USE_GAMEBOY_GNUBOY
  // Note: Magic number obtained by adjusting until audio buffer overflows stop.
  const int audioBufferLength = AUDIO_BUFFER_SIZE;
  displayBuffer[0] = (uint16_t*)get_frame_buffer0();
  displayBuffer[1] = (uint16_t*)get_frame_buffer1();
  audioBuffer[0] = (int32_t*)get_audio_buffer();
  audioBuffer[1] = (int32_t*)get_audio_buffer();

  memset(&fb, 0, sizeof(fb));
  fb.w = 160;
  fb.h = 144;
  fb.pelsize = 2;
  fb.pitch = fb.w * fb.pelsize;
  fb.indexed = 0;
  fb.ptr = (uint8_t*)displayBuffer[0];
  fb.enabled = 1;
  fb.dirty = 0;
  framebuffer = displayBuffer[0];

  // pcm.len = count of 16bit samples (x2 for stereo)
  memset(&pcm, 0, sizeof(pcm));
  pcm.hz = 16000;
  pcm.stereo = 1;
  pcm.len = /*pcm.hz / 2*/ audioBufferLength;
  pcm.buf = (int16_t*)audioBuffer[0];
  pcm.pos = 0;

  sound_reset();

  fmt::print("GAMEBOY enabled: GNUBOY\n");
  loader_init(romdata, rom_data_size);
  emu_reset();
  totalElapsedSeconds = 0;
  frame = 0;
  if (!initialized) {
    gbc_task = std::make_shared<espp::Task>(espp::Task::Config{
        .name = "gbc task",
        .callback = run_to_vblank,
        .stack_size_bytes = 6*1024,
        .priority = 20,
        .core_id = 1
      });
    gbc_video_task = std::make_shared<espp::Task>(espp::Task::Config{
        .name = "gbc video task",
        .callback = video_task,
        .stack_size_bytes = 6*1024,
        .priority = 15,
        .core_id = 1
      });
    video_queue = xQueueCreate(1, sizeof(uint16_t*));
    gbc_video_task->start();
  }
  auto save_path = get_save_path();
  if (save_path.size()) {
    auto f = fopen(save_path.c_str(), "rb");
    loadstate(f);
    fclose(f);
  }
  gbc_task->start();
#endif
  initialized = true;
}

void run_gameboy_rom() {
  // GET INPUT
  get_input_state(&state);
  pad_set(PAD_UP, state.up);
  pad_set(PAD_DOWN, state.down);
  pad_set(PAD_LEFT, state.left);
  pad_set(PAD_RIGHT, state.right);
  pad_set(PAD_SELECT, state.select);
  pad_set(PAD_START, state.start);
  pad_set(PAD_A, state.a);
  pad_set(PAD_B, state.b);
  // handle touchpad so we can know if the user needs to quit
  uint8_t _num_touches, _btn_state;
  uint16_t _x,_y;
  touchpad_read(&_num_touches, &_x, &_y, &_btn_state);
  // don't need to do anything else because the gbc task runs the main display loop
}

void deinit_gameboy() {
  fmt::print("quitting gameboy emulation!\n");
#if USE_GAMEBOY_GNUBOY
  // stop the task...
  gbc_task->stop();
  // save state
  fmt::print("Saving state\n");
  auto save_path = get_save_path(true);
  auto f = fopen(save_path.c_str(), "wb");
  savestate(f);
  fclose(f);
  // now unload everything
  loader_unload();
#endif
}
