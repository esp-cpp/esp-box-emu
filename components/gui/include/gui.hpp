#pragma once

#include <atomic>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

#include "event_manager.hpp"
#include "high_resolution_timer.hpp"
#include "jpeg.hpp"
#include "logger.hpp"

#include "emu-ui/launcher.hpp"

#include "box-emu.hpp"
#include "rom_info.hpp"
#include "settings.hpp"

/// The launcher: the games in metadata.csv on the SD card, grouped by
/// console, with the selected game's boxart, a console picker and the
/// settings (kept in AppSettings). The screens are emu-ui's (shared with
/// tab5-emu); this is the box's side of them: the games, the images, the
/// settings (haptics, the Genesis CPU), the USB drive and the X button.
class Gui {
public:
  typedef std::function<void(void)> play_haptic_fn;
  typedef std::function<void(int)> set_waveform_fn;

  struct Config {
    play_haptic_fn play_haptic;
    set_waveform_fn set_waveform;
    std::string metadata_filename = "metadata.csv";
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  explicit Gui(const Config &config);
  ~Gui();

  void ready_to_play(bool new_state) { ready_to_play_ = new_state; }
  bool ready_to_play() const { return ready_to_play_; }

  std::optional<RomInfo> get_selected_rom() const {
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    if (focused_rom_ < 0 || focused_rom_ >= (int)rom_infos_.size()) {
      return std::nullopt;
    }
    return rom_infos_[focused_rom_];
  }

  /// Stop driving LVGL (while a game is running)
  void pause();
  /// Reload the rom list if the metadata changed, and start driving LVGL again
  void resume();

  int get_haptic_waveform() const { return haptic_waveform_; }
  /// 1..123 (wraps); saved in AppSettings
  void set_haptic_waveform(int new_waveform);

  void update_rom_list();
  /// Re-read metadata.csv even if its modification time did not change (FAT
  /// times are coarse, and without a set clock they may not move at all).
  void reload_rom_list() {
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    metadata_last_modified_ = std::filesystem::file_time_type::min();
    update_rom_list();
  }
  /// The games in list order (grouped by console).
  std::vector<RomInfo> rom_infos() const {
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    return rom_infos_;
  }
  /// Select the first game whose name or rom path contains `text` (a game
  /// the console filter hides lifts the filter)
  bool select_rom_matching(std::string_view text);

  /// The consoles that have games, in list order.
  std::vector<Console> consoles() const;
  /// The console whose games the list shows, if it is filtered.
  std::optional<Console> console_filter() const;
  /// Show only `console`'s games (nullopt: every console's); false if it has none.
  bool set_console_filter(std::optional<Console> console);

  /// The settings the launcher shows, for the USB link: {settings: {name:
  /// value}, limits: {name: [min, max]}} (volume, muted, brightness, video,
  /// resume, haptic).
  std::string settings_json() const;
  /// Change one of them as its control in the launcher does (shown, applied,
  /// saved once the changes stop); false for an unknown name or a value out
  /// of range.
  bool set_setting(const std::string &name, int value);

  /// The launcher screen (RGB565, little endian, rows packed), for the USB
  /// link's screenshot when no game runs; empty if it cannot be drawn.
  struct Snapshot {
    size_t width{0}, height{0};
    std::vector<uint8_t> pixels;
  };
  Snapshot snapshot();

protected:
  std::vector<emu_ui::Row> settings_rows();
  void set_volume(int v);
  void set_mute(bool muted);
  void set_brightness(int v);
  void set_video_setting(int v);
  void set_usb_drive(bool on);
  void update();
  void on_battery(const std::vector<uint8_t> &data);
  void on_settings_changed(const std::vector<uint8_t> &data);
  const void *cover(size_t index);
  const void *console_art(const std::string &key);

  std::vector<RomInfo> rom_infos_;
  std::atomic<int> focused_rom_{-1};
  std::unique_ptr<emu_ui::Launcher> launcher_;

  Jpeg decoder_;               ///< one buffer: the cover is decoded last
  lv_image_dsc_t boxart_{};
  /// boxart/consoles/<key>.jpg, decoded once into PSRAM (data nullptr: none)
  struct Art {
    uint8_t *data{nullptr};
    lv_image_dsc_t desc{};
  };
  std::map<std::string, Art> console_art_;

  play_haptic_fn play_haptic_;
  set_waveform_fn set_waveform_;
  std::atomic<int> haptic_waveform_{12};
  bool usb_drive_shown_{false};
  bool x_down_{false};

  std::string metadata_filename_;
  std::filesystem::file_time_type metadata_last_modified_{};

  std::atomic<bool> paused_{false};
  std::atomic<bool> ready_to_play_{false};
  espp::HighResolutionTimer task_{{
      .name = "Gui Task",
      .callback = std::bind(&Gui::update, this),
  }};
  espp::Logger logger_;
  std::recursive_mutex &mutex_{BoxEmu::lvgl_mutex()}; // shared with the menu (one LVGL instance)
};
