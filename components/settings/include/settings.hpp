#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "persistent_data.hpp"

/// The launcher settings that survive a power cycle, version 1. Kept in
/// settings.bin at the root of the SD card by espp::PersistentData: a new
/// field means a new version struct (with a migrate() from this one) in
/// front of the type list in AppSettings, never a change to this one.
struct SettingsV1 {
  static constexpr uint32_t VERSION = 1;
  uint8_t volume{60};         ///< 0..100
  bool muted{false};
  uint8_t brightness{80};     ///< 0..100
  uint8_t video_setting{1};   ///< VideoSetting (0 original, 1 fit, 2 fill)
  bool resume_last_save{true}; ///< starting a game loads the slot it was last saved to / loaded from
  std::string last_rom;       ///< rom_path of the last game started (selected again at boot)
  std::map<std::string, uint8_t> last_slot; ///< rom_path -> the save slot last saved to / loaded from
  uint8_t haptic_waveform{12}; ///< DRV2605 effect played when a game starts
};

/// Version 2: adds the Genesis 68000 clock.
struct SettingsV2 {
  static constexpr uint32_t VERSION = 2;
  uint8_t volume{60};         ///< 0..100
  bool muted{false};
  uint8_t brightness{80};     ///< 0..100
  uint8_t video_setting{1};   ///< VideoSetting (0 original, 1 fit, 2 fill)
  bool resume_last_save{true}; ///< starting a game loads the slot it was last saved to / loaded from
  std::string last_rom;       ///< rom_path of the last game started (selected again at boot)
  std::map<std::string, uint8_t> last_slot; ///< rom_path -> the save slot last saved to / loaded from
  uint8_t haptic_waveform{12}; ///< DRV2605 effect played when a game starts
  uint8_t genesis_cpu_percent{100}; ///< Genesis 68000 clock, % of stock (125 removes most in-game lag)

  /// The supported Genesis clocks are Normal (100) and 125%; a stored value is
  /// read through this so anything else (an older or hand-edited file) maps to one
  static constexpr uint8_t genesis_cpu_choice(int percent) { return percent > 100 ? 125 : 100; }

  static SettingsV2 migrate(const SettingsV1 &v1) {
    return {.volume = v1.volume,
            .muted = v1.muted,
            .brightness = v1.brightness,
            .video_setting = v1.video_setting,
            .resume_last_save = v1.resume_last_save,
            .last_rom = v1.last_rom,
            .last_slot = v1.last_slot,
            .haptic_waveform = v1.haptic_waveform};
  }
};

/// The current settings version.
using Settings = SettingsV2;

/// The settings file and an in-RAM copy of it. Changes are made in RAM
/// (modify()) and written when save() is called; the launcher calls save()
/// a moment after the last change, so dragging a slider is one write.
/// Thread-safe.
class AppSettings {
public:
  static AppSettings &get() {
    static AppSettings instance;
    return instance;
  }

  /// Load the file, or create it from `defaults` when there is none. A file
  /// that cannot be read (corrupt, or written by newer firmware) is reported
  /// and replaced by `defaults` in RAM; the file itself is only overwritten by
  /// the next save().
  bool init(const std::string &file_path, const Settings &defaults);

  /// A copy of the current settings.
  Settings get_settings() const;

  /// Change the settings in RAM (written by the next save()).
  template <typename Fn> void modify(Fn &&fn) {
    std::lock_guard<std::mutex> lk(mutex_);
    Settings s = data_ ? data_->get() : fallback_;
    const Settings before = s;
    fn(s);
    if (!same(before, s)) {
      if (data_)
        data_->set(s);
      else
        fallback_ = s;
      dirty_ = true;
      last_change_ = std::chrono::steady_clock::now();
    }
  }

  /// Write the settings if they changed since the last save.
  bool save();

  /// save(), but only once `quiet` has passed since the last change (call it
  /// periodically: a burst of changes is written once, after it ends).
  bool save_if_idle(std::chrono::milliseconds quiet = std::chrono::milliseconds(1500));

  /// The save slot last used for `rom_path`, if any.
  std::optional<int> last_slot(const std::string &rom_path) const;
  /// Remember `slot` as the last one used for `rom_path`.
  void set_last_slot(const std::string &rom_path, int slot);

protected:
  AppSettings() = default;
  static bool same(const Settings &a, const Settings &b);

  using Store = espp::PersistentData<SettingsV2, SettingsV1>;
  mutable std::mutex mutex_;
  std::unique_ptr<Store> data_;
  Settings fallback_{}; ///< used when init() was not called (no SD card)
  bool dirty_{false};
  std::chrono::steady_clock::time_point last_change_{};
};
