#include "box_emu_hal.hpp"

static std::atomic<bool> muted_{false};
static std::atomic<int> volume_{60};

static std::shared_ptr<espp::Button> mute_button;
static std::unique_ptr<espp::Task> audio_task;
static TaskHandle_t main_task_handle = NULL;
static i2s_chan_handle_t tx_handle = NULL;

// Scratch buffers
namespace hal {
static constexpr int DEFAULT_AUDIO_RATE = 48000;
static constexpr int NUM_CHANNELS = 2;
static constexpr int NUM_BYTES_PER_CHANNEL = 2;
static constexpr int UPDATE_FREQUENCY = 60;
static constexpr int AUDIO_BUFFER_SIZE = DEFAULT_AUDIO_RATE * NUM_CHANNELS * NUM_BYTES_PER_CHANNEL / UPDATE_FREQUENCY;
}
static uint8_t tx_buffer[hal::AUDIO_BUFFER_SIZE];
static StreamBufferHandle_t tx_audio_stream;

static i2s_std_config_t std_cfg;

static i2s_event_callbacks_t tx_callbacks_;
std::atomic<bool> has_sound{false};

static void update_volume_output() {
  if (muted_) {
    es8311_codec_set_voice_volume(0);
  } else {
    es8311_codec_set_voice_volume(volume_);
  }
}

void hal::set_muted(bool mute) {
  muted_ = mute;
  update_volume_output();
}

bool hal::is_muted() {
  return muted_;
}

void hal::set_audio_volume(int percent) {
  volume_ = percent;
  update_volume_output();
}

int hal::get_audio_volume() {
  return volume_;
}

uint32_t hal::get_audio_sample_rate() {
  return std_cfg.clk_cfg.sample_rate_hz;
}

void hal::set_audio_sample_rate(uint32_t sample_rate) {
  fmt::print("Setting sample rate to {}\n", sample_rate);
  // disable i2s
  i2s_channel_disable(tx_handle);
  // update the config
  std_cfg.clk_cfg.sample_rate_hz = sample_rate;
  i2s_channel_reconfig_std_clock(tx_handle, &std_cfg.clk_cfg);
  // clear the buffer
  xStreamBufferReset(tx_audio_stream);
  // re-enable i2s
  i2s_channel_enable(tx_handle);
}

static bool IRAM_ATTR audio_task_fn(std::mutex &m, std::condition_variable &cv)
{
  // Queue the next I2S out frame to write
  uint16_t available = xStreamBufferBytesAvailable(tx_audio_stream);
  available = std::min<uint16_t>(available, hal::AUDIO_BUFFER_SIZE);
  uint8_t* buffer = &tx_buffer[0];
  static constexpr int BUFF_SIZE = hal::AUDIO_BUFFER_SIZE;
  memset(buffer, 0, BUFF_SIZE);

  if (available == 0) {
    i2s_channel_write(tx_handle, buffer, BUFF_SIZE, NULL, portMAX_DELAY);
  } else {
    xStreamBufferReceive(tx_audio_stream, buffer, available, 0);
    i2s_channel_write(tx_handle, buffer, available, NULL, portMAX_DELAY);
  }
  return false; // don't stop the task
}

static bool IRAM_ATTR tx_sent_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx)
{
  // notify the main task that we're done
  vTaskNotifyGiveFromISR(main_task_handle, NULL);
  return true;
}

static esp_err_t i2s_driver_init(void)
{
  fmt::print("initializing i2s driver...\n");
  auto ret_val = ESP_OK;
  fmt::print("Using newer I2S standard\n");
  i2s_chan_config_t chan_cfg = { \
    .id = i2s_port,
    .role = I2S_ROLE_MASTER,
    .dma_desc_num = 16,
    .dma_frame_num = 48,
    .auto_clear = true,
    .intr_priority = 0,
  };

  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, nullptr));

  std_cfg = {
    .clk_cfg = {
      .sample_rate_hz = hal::DEFAULT_AUDIO_RATE,
      .clk_src = I2S_CLK_SRC_DEFAULT,
      .ext_clk_freq_hz = 0,
      .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    },
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = i2s_mck_io,
      .bclk = i2s_bck_io,
      .ws = i2s_ws_io,
      .dout = i2s_do_io,
      .din = i2s_di_io,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false,
      },
    },
  };
  std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

  ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));

  tx_audio_stream = xStreamBufferCreate(hal::AUDIO_BUFFER_SIZE*4, 0);

  memset(&tx_callbacks_, 0, sizeof(tx_callbacks_));
  tx_callbacks_.on_sent = tx_sent_callback;
  i2s_channel_register_event_callback(tx_handle, &tx_callbacks_, NULL);

  main_task_handle = xTaskGetCurrentTaskHandle();

  audio_task = std::make_unique<espp::Task>(espp::Task::Config{
    .name = "audio task",
    .callback = audio_task_fn,
    .stack_size_bytes = 1024 * 4,
    .priority = 19,
    .core_id = 1,
  });

  xStreamBufferReset(tx_audio_stream);

  ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
  return ret_val;
}

// es8311 is for audio output codec
static esp_err_t es8311_init_default(void)
{
  fmt::print("initializing es8311 codec...\n");
  esp_err_t ret_val = ESP_OK;
  audio_hal_codec_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.codec_mode = AUDIO_HAL_CODEC_MODE_DECODE;
  cfg.dac_output = AUDIO_HAL_DAC_OUTPUT_LINE1;
  cfg.i2s_iface.bits = AUDIO_HAL_BIT_LENGTH_16BITS;
  cfg.i2s_iface.fmt = AUDIO_HAL_I2S_NORMAL;
  cfg.i2s_iface.mode = AUDIO_HAL_MODE_SLAVE;
  cfg.i2s_iface.samples = AUDIO_HAL_16K_SAMPLES;

  ret_val |= es8311_codec_init(&cfg);
  ret_val |= es8311_set_bits_per_sample(cfg.i2s_iface.bits);
  ret_val |= es8311_config_fmt((es_i2s_fmt_t)cfg.i2s_iface.fmt);
  ret_val |= es8311_codec_set_voice_volume(volume_);
  ret_val |= es8311_codec_ctrl_state(cfg.codec_mode, AUDIO_HAL_CTRL_START);

  if (ESP_OK != ret_val) {
    fmt::print("Failed initialize codec\n");
  } else {
    fmt::print("Codec initialized\n");
  }

  return ret_val;
}

static std::unique_ptr<espp::Task> mute_task;
static QueueHandle_t gpio_evt_queue;

static void gpio_isr_handler(void *arg) {
  uint32_t gpio_num = (uint32_t)arg;
  xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void init_mute_button(void) {
  // register that we publish the mute button state
  espp::EventManager::get().add_publisher(mute_button_topic, "i2s_audio");

  fmt::print("Initializing mute button\n");
  mute_button = std::make_shared<espp::Button>(espp::Button::Config{
      .name = "mute button",
      .interrupt_config =
          {
              .gpio_num = mute_pin,
              .callback =
                  [&](const espp::Interrupt::Event &event) {
                    hal::set_muted(event.active);
                    // simply publish that the mute button was presssed
                    espp::EventManager::get().publish(mute_button_topic, {});
                  },
              .active_level = espp::Interrupt::ActiveLevel::LOW,
              .interrupt_type = espp::Interrupt::Type::ANY_EDGE,
              .pullup_enabled = true,
              .pulldown_enabled = false,
          },
      .task_config =
          {
              .name = "mute button task",
              .stack_size_bytes = 4 * 1024,
              .priority = 5,
          },
      .log_level = espp::Logger::Verbosity::WARN,
  });

  // update the mute state (since it's a flip-flop and may have been set if we
  // restarted without power loss)
  hal::set_muted(mute_button->is_pressed());

  fmt::print("Mute button initialized\n");
}

static bool initialized = false;
void hal::audio_init() {
  if (initialized) return;

  // Config power control IO
  gpio_set_direction(sound_power_pin, GPIO_MODE_OUTPUT);
  gpio_set_level(sound_power_pin, 1);

  auto internal_i2c = hal::get_internal_i2c();

  set_es8311_write(std::bind(&espp::I2c::write, internal_i2c.get(),
                             std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  set_es8311_read(std::bind(&espp::I2c::read_at_register, internal_i2c.get(),
                            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

  i2s_driver_init();
  es8311_init_default();

  // now initialize the mute gpio
  init_mute_button();

  audio_task->start();

  fmt::print("Audio initialized\n");

  initialized = true;
}

void IRAM_ATTR hal::play_audio(const uint8_t *data, uint32_t num_bytes) {
  if (has_sound) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
  // don't block here
  xStreamBufferSendFromISR(tx_audio_stream, data, num_bytes, NULL);
  has_sound = true;
}
