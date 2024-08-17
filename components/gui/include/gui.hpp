#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "event_manager.hpp"
#include "display.hpp"
#include "jpeg.hpp"
#include "high_resolution_timer.hpp"
#include "logger.hpp"

#include "box-emu.hpp"
#include "rom_info.hpp"

class Gui {
public:
  typedef std::function<void(void)> play_haptic_fn;
  typedef std::function<void(int)> set_waveform_fn;
  typedef std::function<void(int, int)> set_haptic_slot_fn;

  struct Config {
    play_haptic_fn play_haptic;
    set_waveform_fn set_waveform;
    std::string metadata_filename = "metadata.csv";
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  explicit Gui(const Config& config)
    : play_haptic_(config.play_haptic),
      set_waveform_(config.set_waveform),
      metadata_filename_(config.metadata_filename),
      logger_({.tag="Gui", .level=config.log_level}) {
    init_ui();
    update_shared_state();
    // now start the gui updater task
    task_.periodic(16 * 1000);
    using namespace std::placeholders;
    // register events
    espp::EventManager::get().add_subscriber(mute_button_topic,
                                             "gui",
                                             std::bind(&Gui::on_mute_button_pressed, this, _1),
                                             4*1024);
    espp::EventManager::get().add_subscriber(battery_topic,
                                             "gui",
                                             std::bind(&Gui::on_battery, this, _1),
                                             5*1024);
    espp::EventManager::get().add_subscriber(volume_changed_topic,
                                             "gui",
                                             std::bind(&Gui::on_volume, this, _1),
                                             4*1024);
  }

  ~Gui() {
    espp::EventManager::get().remove_subscriber(mute_button_topic, "gui");
    espp::EventManager::get().remove_subscriber(battery_topic, "gui");
    espp::EventManager::get().remove_subscriber(volume_changed_topic, "gui");
    task_.stop();
    deinit_ui();
  }

  void ready_to_play(bool new_state) {
    ready_to_play_ = new_state;
  }

  bool ready_to_play() const {
    return ready_to_play_;
  }

  void set_mute(bool muted);

  void toggle_mute() {
    set_mute(!espp::EspBox::get().is_muted());
  }

  void set_audio_level(int new_audio_level);

  void set_brightness(int new_brightness);

  void set_video_setting(VideoSetting setting);

  void clear_rom_list();

  void add_rom(const RomInfo& rom);

  std::optional<RomInfo> get_selected_rom() const {
    if (focused_rom_ < 0 || focused_rom_ >= rom_infos_.size()) {
      return std::nullopt;
    }
    return rom_infos_[focused_rom_];
  }

  void pause() {
    freeze_focus();
    paused_ = true;
  }
  void resume() {
    update_shared_state();
    paused_ = false;
    focus_rommenu();
  }

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

  void update_rom_list();

protected:
  void init_ui();
  void deinit_ui();

  void freeze_focus();
  void focus_rommenu();
  void focus_settings();

  void load_rom_screen();
  void load_settings_screen();

  void toggle_usb();

  void update_shared_state() {
    auto &box = espp::EspBox::get();
    set_mute(box.is_muted());
    set_audio_level(box.volume());
    set_brightness(box.brightness());
    set_video_setting(BoxEmu::get().video_setting());
  }

  VideoSetting get_video_setting();

  void on_rom_focused(int index);

  void on_mute_button_pressed(const std::vector<uint8_t>& data) {
    set_mute(espp::EspBox::get().is_muted());
  }

  void on_battery(const std::vector<uint8_t>& data);

  void on_volume(const std::vector<uint8_t>& data);

  lv_image_dsc_t make_boxart(const std::string& path) {
    // load the file
    decoder_.decode(path.c_str());
    // make the descriptor
    lv_image_dsc_t img_desc;
    memset(&img_desc, 0, sizeof(img_desc));
    img_desc.header.cf = LV_COLOR_FORMAT_NATIVE;
    img_desc.header.w = decoder_.get_width();
    img_desc.header.h = decoder_.get_height();
    img_desc.data_size = decoder_.get_size();
    img_desc.data = decoder_.get_decoded_data();
    // and return it
    return img_desc;
  }

  void update() {
    if (!paused_) {
      std::lock_guard<std::recursive_mutex> lk(mutex_);
      lv_task_handler();
    }
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
    case LV_EVENT_SCROLL:
      gui->on_scroll(e);
      break;
    case LV_EVENT_PRESSED:
    case LV_EVENT_CLICKED:
      gui->on_pressed(e);
      break;
    case LV_EVENT_VALUE_CHANGED:
      gui->on_value_changed(e);
      break;
    case LV_EVENT_LONG_PRESSED:
      break;
    case LV_EVENT_KEY:
      gui->on_key(e);
      break;
    default:
      break;
    }
  }

  void on_pressed(lv_event_t *e);
  void on_value_changed(lv_event_t *e);
  void on_key(lv_event_t *e);
  void on_scroll(lv_event_t *e);

  // LVLG gui objects
  std::vector<RomInfo> rom_infos_;
  std::atomic<int> focused_rom_{-1};
  lv_image_dsc_t focused_boxart_;

  // style for buttons
  lv_style_t button_style_;

  lv_anim_t rom_label_animation_template_;
  lv_style_t rom_label_style_;

  lv_group_t *rom_screen_group_;
  lv_group_t *settings_screen_group_;

  Jpeg decoder_;

  play_haptic_fn play_haptic_;
  set_waveform_fn set_waveform_;
  std::atomic<int> haptic_waveform_{12};

  // info for tracking when the metadata was changed last. Right now we use the
  // last modified time of the metadata file, but we could also use a hash of
  // the metadata file
  std::string metadata_filename_;
  std::filesystem::file_time_type metadata_last_modified_;

  std::atomic<bool> paused_{false};
  std::atomic<bool> ready_to_play_{false};
  espp::HighResolutionTimer task_{{
      .name = "Gui Task",
      .callback = std::bind(&Gui::update, this),
    }};
  espp::Logger logger_;
  std::recursive_mutex mutex_;
};
