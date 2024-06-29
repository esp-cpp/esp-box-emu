#include "box_emu_hal.hpp"

using namespace hal;

static std::shared_ptr<espp::Task> video_task_;
static QueueHandle_t video_queue_;

static size_t display_width = lcd_width;
static size_t display_height = lcd_height;

static size_t native_width = lcd_width;
static size_t native_height = lcd_height;
static int native_pitch = lcd_width;

static const uint16_t* palette = nullptr;
static size_t palette_size = 256;

static bool video_task(std::mutex &m, std::condition_variable& cv);

void hal::init_video_task() {
  static bool initialized = false;
  if (initialized) {
    return;
  }
  fmt::print("initializing video task...\n");
  video_queue_ = xQueueCreate(1, sizeof(uint16_t*));
  video_task_ = std::make_shared<espp::Task>(espp::Task::Config{
      .name = "video task",
      .callback = video_task,
      .stack_size_bytes = 4*1024,
      .priority = 20,
      .core_id = 1
    });
  video_task_->start();
  initialized = true;
}

void hal::set_display_size(size_t width, size_t height) {
  display_width = width;
  display_height = height;
}

void hal::set_native_size(size_t width, size_t height, int pitch) {
  native_width = width;
  native_height = height;
  native_pitch = pitch == -1 ? width : pitch;
}

void hal::set_palette(const uint16_t* _palette, size_t size) {
  palette = _palette;
  palette_size = size;
}

void hal::push_frame(const void* frame) {
  if (video_queue_ == nullptr) {
    fmt::print("video queue is null, make sure to call init_video_task() first\n");
    return;
  }
  xQueueSend(video_queue_, &frame, 10 / portTICK_PERIOD_MS);
}

static bool has_palette() {
  return palette != nullptr;
}

static bool is_native() {
  return native_width == display_width && native_height == display_height;
}

static int get_x_offset() {
  return (lcd_width-display_width)/2;
}

static int get_y_offset() {
  return (lcd_height-display_height)/2;
}

static const uint16_t* get_palette() {
  return palette;
}

static bool video_task(std::mutex &m, std::condition_variable& cv) {
  const void *_frame_ptr;
  if (xQueuePeek(video_queue_, &_frame_ptr, 100 / portTICK_PERIOD_MS) != pdTRUE) {
    // we couldn't get anything from the queue, return
    return false;
  }
  if (_frame_ptr == nullptr) {
    // make sure we clear the queue
    xQueueReceive(video_queue_, &_frame_ptr, 10 / portTICK_PERIOD_MS);
    // we got a nullptr, return
    return false;
  }
  static constexpr int num_lines_to_write = NUM_ROWS_IN_FRAME_BUFFER;
  static int vram_index = 0; // has to be static so that it persists between calls
  const int x_offset = get_x_offset();
  const int y_offset = get_y_offset();
  const uint16_t* _palette = get_palette();
  if (is_native()) {
    for (int y=0; y<display_height; y+= num_lines_to_write) {
      uint16_t* _buf = vram_index ? (uint16_t*)get_vram1() : (uint16_t*)get_vram0();
      vram_index = vram_index ? 0 : 1;
      int num_lines = std::min<int>(num_lines_to_write, display_height-y);
      if (has_palette()) {
        const uint8_t* _frame = (const uint8_t*)_frame_ptr;
        for (int i=0; i<num_lines; i++) {
          // write two pixels (32 bits) at a time because it's faster
          for (int j=0; j<display_width/2; j++) {
            int src_index = (y+i)*native_pitch + j * 2;
            int dst_index = i*display_width + j * 2;
            _buf[dst_index] = _palette[_frame[src_index] % palette_size];
            _buf[dst_index + 1] = _palette[_frame[src_index + 1] % palette_size];
          }
        }
      } else {
        const uint16_t* _frame = (const uint16_t*)_frame_ptr;
        for (int i=0; i<num_lines; i++) {
          // write two pixels (32 bits) at a time because it's faster
          for (int j=0; j<display_width/2; j++) {
            int src_index = (y+i)*native_pitch + j * 2;
            int dst_index = i*display_width + j * 2;
            // memcpy(&_buf[i*display_width + j * 2], &_frame[(y+i)*native_pitch + j * 2], 4);
            _buf[dst_index] = _frame[src_index];
            _buf[dst_index + 1] = _frame[src_index + 1];
          }
        }
      }
      lcd_write_frame(x_offset, y + y_offset, display_width, num_lines, (uint8_t*)&_buf[0]);
    }
  } else {
    // we are scaling the screen (and possibly using a custom palette)
    // if we don't have a custom palette, we just need to scale/fill the frame
    [[maybe_unused]] float y_scale = (float)display_height/native_height;
    float x_scale = (float)display_width/native_width;
    float inv_x_scale = (float)native_width/display_width;
    float inv_y_scale = (float)native_height/display_height;
    int max_y = lcd_height;
    int max_x = std::clamp<int>(x_scale * native_width, 0, lcd_width);
    for (int y=0; y<max_y; y+=num_lines_to_write) {
      // each iteration of the loop, we swap the vram index so that we can
      // write to the other buffer while the other one is being transmitted
      int i = 0;
      uint16_t* _buf = vram_index ? (uint16_t*)get_vram1() : (uint16_t*)get_vram0();
      vram_index = vram_index ? 0 : 1;
      for (; i<num_lines_to_write; i++) {
        int _y = y+i;
        if (_y >= max_y) {
          break;
        }
        int source_y = (float)_y * inv_y_scale;
        // shoudl i put this around the outer loop or is this loop a good
        // balance for perfomance of the check?
        if (has_palette()) {
          const uint8_t* _frame = (const uint8_t*)_frame_ptr;
          // write two pixels (32 bits) at a time because it's faster
          for (int x=0; x<max_x/2; x++) {
            int source_x = (float)x * 2 * inv_x_scale;
            int src_index = source_y*native_pitch + source_x;
            int dst_index = i*max_x + x * 2;
            _buf[dst_index] = _palette[_frame[src_index] % palette_size];
            _buf[dst_index + 1] = _palette[_frame[src_index + 1] % palette_size];
          }
        } else {
          const uint16_t* _frame = (const uint16_t*)_frame_ptr;
          // write two pixels (32 bits) at a time because it's faster
          for (int x=0; x<max_x/2; x++) {
            int source_x = (float)x * 2 * inv_x_scale;
            int src_index = source_y*native_pitch + source_x;
            int dst_index = i*max_x + x * 2;
            _buf[dst_index] = _frame[src_index];
            _buf[dst_index + 1] = _frame[src_index + 1];
          }
        }
      }
      lcd_write_frame(0 + x_offset, y, max_x, i, (uint8_t*)&_buf[0]);
    }
  }

  // we don't have to worry here since we know there was an item in the queue
  // since we peeked earlier.
  xQueueReceive(video_queue_, &_frame_ptr, 10 / portTICK_PERIOD_MS);
  return false;
}
