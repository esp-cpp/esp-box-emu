#include "audio_task.hpp"

using namespace hal;

static size_t audio_sample_count = 0;
static std::shared_ptr<espp::Task> audio_task_;
static QueueHandle_t audio_queue_;

static bool audio_task(std::mutex &m, std::condition_variable& cv);

void hal::init_audio_task() {
  static bool initialized = false;
  if (initialized) {
    return;
  }
  audio_queue_ = xQueueCreate(1, sizeof(uint16_t*));
  audio_task_ = std::make_shared<espp::Task>(espp::Task::Config{
      .name = "audio task",
      .callback = audio_task,
      .stack_size_bytes = 4*1024,
      .priority = 20,
      .core_id = 1
    });
  audio_task_->start();
  initialized = true;
}

void hal::set_audio_sample_count(size_t count) {
  audio_sample_count = count;
}

void hal::push_audio(const void* audio) {
  if (audio_queue_ == nullptr) {
    fmt::print("audio queue is null, make sure to call init_audio_task() first\n");
    return;
  }
  xQueueSend(audio_queue_, &audio, 10 / portTICK_PERIOD_MS);
}

static bool audio_task(std::mutex &m, std::condition_variable& cv) {
  const void *_audio_ptr;
  if (xQueuePeek(audio_queue_, &_audio_ptr, 100 / portTICK_PERIOD_MS) != pdTRUE) {
    // we couldn't get anything from the queue, return
    return false;
  }
  if (_audio_ptr == nullptr) {
    // make sure we clear the queue
    xQueueReceive(audio_queue_, &_audio_ptr, 10 / portTICK_PERIOD_MS);
    // we got a nullptr, return
    return false;
  }
  // we got a valid audio frame, so let's play it
  const uint8_t* audio_ptr = (const uint8_t*)_audio_ptr;
  // multiply by 2 since we're 16-bit
  audio_play_frame(audio_ptr, audio_sample_count*2);

  // we don't have to worry here since we know there was an item in the queue
  // since we peeked earlier.
  xQueueReceive(audio_queue_, &_audio_ptr, 10 / portTICK_PERIOD_MS);
  return false;
}
