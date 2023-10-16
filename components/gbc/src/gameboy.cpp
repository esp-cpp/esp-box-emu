// #pragma GCC optimize ("Ofast")

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
bool video_task(std::mutex &m, std::condition_variable& cv) {
  static uint16_t *_frame;
  if (xQueuePeek(video_queue, &_frame, 10 / portTICK_PERIOD_MS) != pdTRUE) {
    // we couldn't get anything from the queue, return
    return false;
  }

  if (scaled || filled) {
    static int vram_index = 0;
    int x_offset = filled ? 0 : (320-266)/2;
    int y_offset = 0;
    float y_scale = 1.667f;
    float x_scale = scaled ? y_scale : 2.0f;
    int max_y = (int)(y_scale * 144.0f);
    int max_x = (int)(x_scale * 160.0f);

    static constexpr int num_lines_to_write = NUM_ROWS_IN_FRAME_BUFFER;
    for (int y=0; y<max_y; y+=num_lines_to_write) {
      uint16_t* _buf = vram_index ? (uint16_t*)get_vram1() : (uint16_t*)get_vram0();
      vram_index = vram_index ? 0 : 1;
      int i = 0;
      for (; i<num_lines_to_write; i++) {
        int _y = y+i;
        if (_y >= max_y) {
          break;
        }
        int source_y = (float)_y/y_scale;
        for (int x=0; x<max_x; x++) {
          int source_x = (float)x/x_scale;
          _buf[i*max_x + x] = _frame[source_y*160 + source_x];
        }
      }
      lcd_write_frame(0 + x_offset, y, max_x, i, (uint8_t*)&_buf[0]);
    }
  } else {
    constexpr int x_offset = (320-160)/2;
    constexpr int y_offset = (240-144)/2;
    static constexpr int num_lines_to_write = NUM_ROWS_IN_FRAME_BUFFER;
    for (int y=0; y<144; y+= num_lines_to_write) {
      int num_lines = std::min(num_lines_to_write, 144-y);
      lcd_write_frame(x_offset, y + y_offset, 160, num_lines, (uint8_t*)&_frame[y*160]);
    }
  }
  // we don't have to worry here since we know there was an item in the queue
  // since we peeked earlier.
  xQueueReceive(video_queue, &_frame, portMAX_DELAY);
  return false;
}

bool run_to_vblank(std::mutex &m, std::condition_variable& cv) {
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
  return false;
}

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

void reset_gameboy() {
  emu_reset();
}

void init_gameboy(const std::string& rom_filename, uint8_t *romdata, size_t rom_data_size) {
  static bool initialized = false;

  // lcd_set_queued_transmit();
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
  }
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
  // don't need to do anything else because the gbc task runs the main display loop
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
  // stop the task...
  gbc_task->stop();
  gbc_video_task->stop();
}

void start_gameboy_tasks() {
  // stop the task...
  gbc_task->start();
  gbc_video_task->start();
}

std::vector<uint8_t> get_gameboy_video_buffer() {
  uint8_t* frame_buffer = get_frame_buffer0();
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
