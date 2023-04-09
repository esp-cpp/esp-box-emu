#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "display.hpp"
#include "jpeg.hpp"
#include "task.hpp"
#include "logger.hpp"

class Gui {
public:
  typedef std::function<void(void)> play_haptic_fn;
  typedef std::function<void(int)> set_waveform_fn;
  typedef std::function<void(int, int)> set_haptic_slot_fn;

  struct Config {
    play_haptic_fn play_haptic;
    set_waveform_fn set_waveform;
    std::shared_ptr<espp::Display> display;
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  enum class VideoSetting { ORIGINAL, FIT, FILL, MAX_UNUSED };

  Gui(const Config& config)
    : play_haptic_(config.play_haptic),
      set_waveform_(config.set_waveform),
      display_(config.display),
      logger_({.tag="Gui", .level=config.log_level}) {
    init_ui();
    // now start the gui updater task
    using namespace std::placeholders;
    task_ = espp::Task::make_unique({
        .name = "Gui Task",
        .callback = std::bind(&Gui::update, this, _1, _2),
        .stack_size_bytes = 6 * 1024
      });
    task_->start();
  }

  void ready_to_play(bool new_state) {
    ready_to_play_ = new_state;
  }

  bool ready_to_play() {
    return ready_to_play_;
  }

  void set_mute(bool muted);

  void toggle_mute() {
    set_mute(!muted_);
  }

  void set_audio_level(int new_audio_level);

  int get_audio_level() {
    if (muted_) return 0;
    return audio_level_;
  }

  void next_video_setting();

  void prev_video_setting();

  void set_video_setting(VideoSetting setting);

  VideoSetting get_video_setting();

  void add_rom(const std::string& name, const std::string& image_path);

  size_t get_selected_rom_index() {
    return focused_rom_;
  }

  void pause() {
    paused_ = true;
  }
  void resume() {
    paused_ = false;
    load_rom_screen();
  }

  void next() {
    // protect since this function is called from another thread context
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    if (roms_.size() == 0) {
      return;
    }
    // focus the next rom
    focused_rom_++;
    if (focused_rom_ >= roms_.size()) focused_rom_ = 0;
    auto rom = roms_[focused_rom_];
    focus_rom(rom);
  }

  void previous() {
    // protect since this function is called from another thread context
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    if (roms_.size() == 0) {
      return;
    }
    // focus the previous rom
    focused_rom_--;
    if (focused_rom_ < 0) focused_rom_ = roms_.size() - 1;
    auto rom = roms_[focused_rom_];
    focus_rom(rom);
  }

  void focus_rom(lv_obj_t *new_focus, bool scroll_to_view=true);

  void set_haptic_waveform(int new_waveform) {
    if (new_waveform > 123) {
      new_waveform = 1;
    } else if (new_waveform <= 0) {
      new_waveform = 123;
    }
    haptic_waveform_ = new_waveform;
    set_waveform_(haptic_waveform_);
    update_haptic_waveform_label();
  }

  void next_haptic_waveform() {
    set_haptic_waveform(haptic_waveform_ + 1);
  }

  void previous_haptic_waveform() {
    set_haptic_waveform(haptic_waveform_ - 1);
  }

  void update_haptic_waveform_label();

protected:
  void init_ui();

  void load_rom_screen();

  lv_img_dsc_t make_boxart(const std::string& path) {
    // load the file
    // auto start = std::chrono::high_resolution_clock::now();
    decoder_.decode(path.c_str());
    // auto end = std::chrono::high_resolution_clock::now();
    // auto elapsed = std::chrono::duration<float>(end-start).count();
    // fmt::print("Decoding took {:.3f}s\n", elapsed);
    // make the descriptor
    lv_img_dsc_t img_desc = {
      .header = {
        .cf = LV_IMG_CF_TRUE_COLOR,
        .always_zero = 0,
        .reserved = 0,
        .w = (uint32_t)decoder_.get_width(),
        .h = (uint32_t)decoder_.get_height(),
      },
      .data_size = (uint32_t)decoder_.get_size(),
      .data = decoder_.get_decoded_data(),
    };
    // and return it
    return img_desc;
  }

  bool update(std::mutex& m, std::condition_variable& cv) {
    if (!paused_) {
      std::lock_guard<std::recursive_mutex> lk(mutex_);
      lv_task_handler();
    }
    {
      using namespace std::chrono_literals;
      std::unique_lock<std::mutex> lk(m);
      cv.wait_for(lk, 16ms);
    }
    // don't want to stop the task
    return false;
  }

  static void event_callback(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);
    auto user_data = lv_event_get_user_data(e);
    auto gui = static_cast<Gui*>(user_data);
    if (!gui) {
      return;
    }
    switch (event_code) {
    case LV_EVENT_SHORT_CLICKED:
      break;
    case LV_EVENT_PRESSED:
      gui->on_pressed(e);
      break;
    case LV_EVENT_LONG_PRESSED:
      break;
    case LV_EVENT_KEY:
      break;
    default:
      break;
    }
  }

  void on_pressed(lv_event_t *e);

  // LVLG gui objects
  std::atomic<bool> muted_{false};
  std::atomic<int> audio_level_{60};
  std::vector<std::string> boxart_paths_;
  std::vector<lv_obj_t*> roms_;
  std::atomic<int> focused_rom_{-1};
  lv_img_dsc_t focused_boxart_;

  lv_anim_t rom_label_animation_template_;
  lv_style_t rom_label_style_;

  Jpeg decoder_;

  play_haptic_fn play_haptic_;
  set_waveform_fn set_waveform_;
  std::atomic<int> haptic_waveform_{16}; // for the DRV2605, this is a 1s alert

  std::atomic<bool> ready_to_play_{false};
  std::atomic<bool> paused_{false};
  std::shared_ptr<espp::Display> display_;
  std::unique_ptr<espp::Task> task_;
  espp::Logger logger_;
  std::recursive_mutex mutex_;
};
