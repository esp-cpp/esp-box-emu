#include "settings.hpp"

#include <tuple>

#include "format.hpp"

bool AppSettings::same(const Settings &a, const Settings &b) {
  return std::tie(a.volume, a.muted, a.brightness, a.video_setting, a.resume_last_save, a.last_rom, a.last_slot,
                  a.haptic_waveform) ==
         std::tie(b.volume, b.muted, b.brightness, b.video_setting, b.resume_last_save, b.last_rom, b.last_slot,
                  b.haptic_waveform);
}

bool AppSettings::init(const std::string &file_path, const Settings &defaults) {
  std::lock_guard<std::mutex> lk(mutex_);
  data_ = std::make_unique<Store>(Store::Config{.file_path = file_path, .auto_load = false});
  std::error_code ec;
  if (data_->load_or_create(defaults, ec)) {
    dirty_ = false;
    return true;
  }
  fmt::print("settings: cannot load {} ({}); using defaults\n", file_path, ec.message());
  data_->set(defaults);
  dirty_ = false;
  return false;
}

Settings AppSettings::get_settings() const {
  std::lock_guard<std::mutex> lk(mutex_);
  return data_ ? data_->get() : fallback_;
}

bool AppSettings::save() {
  std::lock_guard<std::mutex> lk(mutex_);
  if (!dirty_ || !data_) {
    return true;
  }
  std::error_code ec;
  if (!data_->save(ec)) {
    fmt::print("settings: save failed: {}\n", ec.message());
    return false;
  }
  dirty_ = false;
  return true;
}

bool AppSettings::save_if_idle(std::chrono::milliseconds quiet) {
  {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!dirty_ || std::chrono::steady_clock::now() - last_change_ < quiet) {
      return true;
    }
  }
  return save();
}

std::optional<int> AppSettings::last_slot(const std::string &rom_path) const {
  std::lock_guard<std::mutex> lk(mutex_);
  const Settings s = data_ ? data_->get() : fallback_;
  if (auto it = s.last_slot.find(rom_path); it != s.last_slot.end()) {
    return it->second;
  }
  return std::nullopt;
}

void AppSettings::set_last_slot(const std::string &rom_path, int slot) {
  modify([&](Settings &s) { s.last_slot[rom_path] = static_cast<uint8_t>(slot); });
}
