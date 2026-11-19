#include "gui.hpp"

#include <algorithm>
#include <cmath>

#include <esp_app_desc.h>

#include "battery_info.hpp"
#include "events.hpp"

namespace {

// the Console of a console key (rom_info's ConsoleInfo::key)
std::optional<Console> console_of_key(const std::string &key) {
  for (int c = 0; c < static_cast<int>(Console::UNKNOWN); c++) {
    if (key == console_info(static_cast<Console>(c)).key) return static_cast<Console>(c);
  }
  return std::nullopt;
}

struct SettingLimit {
  const char *name;
  int min, max;
};
// (brightness from 10: the launcher's own control stops there too)
constexpr SettingLimit kSettingLimits[] = {{"volume", 0, 100}, {"muted", 0, 1}, {"brightness", 10, 100},
                                           {"video", 0, 2},    {"resume", 0, 1}, {"haptic", 1, 123}};

} // namespace

Gui::Gui(const Config &config)
    : play_haptic_(config.play_haptic)
    , set_waveform_(config.set_waveform)
    , metadata_filename_(config.metadata_filename)
    , logger_({.tag = "Gui", .level = config.log_level}) {
  haptic_waveform_ = std::clamp<int>(AppSettings::get().get_settings().haptic_waveform, 1, 123);
  {
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    auto keypad = BoxEmu::get().keypad();
    launcher_ = std::make_unique<emu_ui::Launcher>(emu_ui::Launcher::Config{
        .metrics = emu_ui::Metrics::compact_320x240(),
        .brand = "esp-box-emu",
        .keypad = keypad ? keypad->get_input_device() : nullptr,
        .cover = [this](size_t i) { return cover(i); },
        .console_art = [this](const std::string &key) { return console_art(key); },
        .on_play =
            [this](size_t i) {
              focused_rom_ = i;
              ready_to_play_ = true;
            },
        .on_select = [this](size_t i) { focused_rom_ = i; },
        .settings = settings_rows(),
    });
    update_rom_list();
    launcher_->show();
  }
  task_.periodic(16 * 1000);
  using namespace std::placeholders;
  auto &events = espp::EventManager::get();
  events.add_subscriber(mute_button_topic, "gui", std::bind(&Gui::on_settings_changed, this, _1), 4 * 1024);
  events.add_subscriber(volume_changed_topic, "gui", std::bind(&Gui::on_settings_changed, this, _1), 4 * 1024);
  events.add_subscriber(battery_topic, "gui", std::bind(&Gui::on_battery, this, _1), 5 * 1024);
}

Gui::~Gui() {
  auto &events = espp::EventManager::get();
  events.remove_subscriber(mute_button_topic, "gui");
  events.remove_subscriber(volume_changed_topic, "gui");
  events.remove_subscriber(battery_topic, "gui");
  task_.stop();
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  launcher_.reset();
  for (auto &[key, art] : console_art_) {
    if (art.data) heap_caps_free(art.data);
  }
}

void Gui::pause() {
  paused_ = true;
  task_.stop();
  // wait out a timer callback already in lv_task_handler() (stop() does not)
  std::lock_guard<std::recursive_mutex> lk(mutex_);
}

void Gui::resume() {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  // (a game started from the PC, emulink launch, may have left the picker open)
  launcher_->close_picker();
  update_rom_list();
  launcher_->refresh_settings();
  launcher_->show();
  // the game drew to the panel itself: repaint all of it
  lv_obj_invalidate(launcher_->screen());
  task_.periodic(16 * 1000);
  paused_ = false;
}

void Gui::update() {
  if (paused_) return;
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  // follow a USB drive hand-over made elsewhere (the USB link's test control)
  const bool on = BoxEmu::get().is_usb_enabled();
  if (on != usb_drive_shown_) {
    usb_drive_shown_ = on;
    launcher_->set_play_enabled(!on);
    launcher_->refresh_settings();
    if (!on) reload_rom_list();
  }
  // X opens (or closes) the console picker: the keypad input has no key for
  // it (not the latched state, the keypad reads that)
  const bool x = BoxEmu::get().gamepad_state().x;
  if (x && !x_down_) launcher_->press(LV_KEY_HOME);
  x_down_ = x;
  lv_task_handler();
}

void Gui::set_volume(int v) {
  v = std::clamp(v, 0, 100);
  BoxEmu::get().volume(v);
  AppSettings::get().modify([v](Settings &s) { s.volume = v; });
}

void Gui::set_mute(bool muted) {
  BoxEmu::get().mute(muted);
  AppSettings::get().modify([muted](Settings &s) { s.muted = muted; });
}

void Gui::set_brightness(int v) {
  v = std::clamp(v, 10, 100);
  BoxEmu::get().brightness((float)v);
  AppSettings::get().modify([v](Settings &s) { s.brightness = v; });
}

void Gui::set_video_setting(int v) {
  BoxEmu::get().video_setting(static_cast<VideoSetting>(v));
  AppSettings::get().modify([v](Settings &s) { s.video_setting = static_cast<uint8_t>(v); });
}

void Gui::set_haptic_waveform(int new_waveform) {
  if (new_waveform > 123) new_waveform = 1;
  else if (new_waveform <= 0) new_waveform = 123;
  haptic_waveform_ = new_waveform;
  if (set_waveform_) set_waveform_(new_waveform);
  AppSettings::get().modify([new_waveform](Settings &s) { s.haptic_waveform = new_waveform; });
  if (launcher_) launcher_->refresh_settings();
}

std::vector<emu_ui::Row> Gui::settings_rows() {
  using K = emu_ui::Row::Kind;
  auto &emu = BoxEmu::get();
  // (saved to the SD card by the main loop once the changes stop)
  std::vector<emu_ui::Row> rows;
  rows.push_back({.label = "Volume", .kind = K::SLIDER, .get = [&emu] { return (int)std::lround(emu.volume()); },
                  .set = [this](int v) { set_volume(v); }, .symbol = LV_SYMBOL_VOLUME_MAX});
  rows.push_back({.label = "Mute", .kind = K::TOGGLE, .get = [&emu] { return emu.is_muted() ? 1 : 0; },
                  .set = [this](int v) { set_mute(v != 0); }, .symbol = LV_SYMBOL_MUTE});
  rows.push_back({.label = "Brightness", .kind = K::SLIDER, .min = 10,
                  .get = [&emu] { return (int)std::lround(emu.brightness()); },
                  .set = [this](int v) { set_brightness(v); }, .symbol = LV_SYMBOL_EYE_OPEN});
  rows.push_back({.label = "Video", .kind = K::CHOICE, .choices = {"Original", "Fit", "Fill"},
                  .get = [&emu] { return (int)emu.video_setting(); }, .set = [this](int v) { set_video_setting(v); },
                  .symbol = LV_SYMBOL_IMAGE});
  rows.push_back({.label = "Resume last save", .kind = K::TOGGLE,
                  .get = [] { return AppSettings::get().get_settings().resume_last_save ? 1 : 0; },
                  .set = [](int v) { AppSettings::get().modify([v](Settings &s) { s.resume_last_save = v != 0; }); },
                  .symbol = LV_SYMBOL_REFRESH});
  rows.push_back({.label = "Haptic", .kind = K::NUMBER, .min = 1, .max = 123, .step = 1,
                  .get = [this] { return haptic_waveform_.load(); },
                  .set = [this](int v) { set_haptic_waveform(v); }, .symbol = LV_SYMBOL_BELL});
  rows.push_back({.label = "Test haptic", .kind = K::ACTION,
                  .set = [this](int) {
                    if (play_haptic_) play_haptic_();
                  },
                  .symbol = LV_SYMBOL_PLAY});
  // the Genesis 68000 clock (125% removes most in-game slowdown, e.g. Sonic
  // losing rings, at the cost of fewer frames drawn)
  rows.push_back({.label = "Genesis CPU", .kind = K::CHOICE, .choices = {"Normal", "125%"},
                  .get = [] { return Settings::genesis_cpu_choice(AppSettings::get().get_settings().genesis_cpu_percent) > 100 ? 1 : 0; },
                  .set = [](int v) {
                    const uint8_t choice = Settings::genesis_cpu_choice(v ? 125 : 100);
                    AppSettings::get().modify([choice](Settings &s) { s.genesis_cpu_percent = choice; });
                  },
                  .symbol = LV_SYMBOL_CHARGE});
  rows.push_back({.label = "USB drive", .kind = K::TOGGLE, .get = [&emu] { return emu.is_usb_enabled() ? 1 : 0; },
                  .set = [this](int v) { set_usb_drive(v != 0); },
                  .note =
                      [&emu] {
                        return std::string(emu.is_usb_enabled() ? "Eject it on the PC, then switch this off."
                                                                : "The SD card as a USB drive on the USB-C port");
                      },
                  .symbol = LV_SYMBOL_USB});
  return rows;
}

void Gui::set_usb_drive(bool on) {
  auto &emu = BoxEmu::get();
  if (on) emu.initialize_usb();
  else emu.deinitialize_usb();
  usb_drive_shown_ = emu.is_usb_enabled();
  launcher_->set_play_enabled(!usb_drive_shown_);
  // the card is ours again: pick up what the PC copied over
  if (!usb_drive_shown_) reload_rom_list();
}

void Gui::on_settings_changed(const std::vector<uint8_t> &) {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  // (the mute button: kept like a change from the launcher)
  set_mute(BoxEmu::get().is_muted());
  launcher_->refresh_settings();
}

void Gui::on_battery(const std::vector<uint8_t> &data) {
  std::error_code ec;
  auto info = espp::deserialize<BatteryInfo>(data, ec);
  if (ec) return;
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  launcher_->set_battery((int)std::lround(info.level), info.charge_rate > 0.0f);
}

void Gui::update_rom_list() {
  // only when the metadata changed since the last time (its modification time)
  std::error_code ec;
  const auto path = std::string(BoxEmu::mount_point) + "/" + metadata_filename_;
  const auto mtime = std::filesystem::last_write_time(path, ec);
  if (ec) {
    logger_.error("Error getting last modified time for {}: {}", path, ec.message());
    return;
  }
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  if (mtime == metadata_last_modified_ && !rom_infos_.empty()) {
    return;
  }
  metadata_last_modified_ = mtime;
  // keep the selected game selected (its index may change); at boot that is
  // the last game played
  std::string selected_path = AppSettings::get().get_settings().last_rom;
  if (focused_rom_ >= 0 && focused_rom_ < (int)rom_infos_.size()) {
    selected_path = rom_infos_[focused_rom_].rom_path;
  }
  rom_infos_.clear();
  // drop duplicates (the same line twice in metadata.csv)
  for (const auto &rom : parse_metadata(metadata_filename_)) {
    if (std::find(rom_infos_.begin(), rom_infos_.end(), rom) == rom_infos_.end()) rom_infos_.push_back(rom);
  }
  // grouped by console, in Console order; metadata.csv order within a console
  std::stable_sort(rom_infos_.begin(), rom_infos_.end(), [](const RomInfo &a, const RomInfo &b) {
    return console_of(a.platform) < console_of(b.platform);
  });
  int selected = -1;
  std::vector<emu_ui::Game> games;
  games.reserve(rom_infos_.size());
  for (size_t i = 0; i < rom_infos_.size(); i++) {
    const auto &rom = rom_infos_[i];
    const auto &info = console_info(console_of(rom.platform));
    games.push_back({.title = rom.name, .console_key = info.key, .console_name = info.name,
                     .console_short = info.short_name});
    if (selected < 0 && rom.rom_path == selected_path) selected = i;
  }
  focused_rom_ = -1;
  launcher_->set_games(std::move(games), selected);
  focused_rom_ = launcher_->selected();
}

std::vector<Console> Gui::consoles() const {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  std::vector<Console> out;
  for (const auto &key : launcher_->consoles()) {
    if (auto c = console_of_key(key)) out.push_back(*c);
  }
  return out;
}

std::optional<Console> Gui::console_filter() const {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  const auto f = launcher_->filter();
  return f ? console_of_key(*f) : std::nullopt;
}

bool Gui::set_console_filter(std::optional<Console> console) {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  return launcher_->set_filter(console ? std::optional<std::string>(console_info(*console).key) : std::nullopt);
}

bool Gui::select_rom_matching(std::string_view text) {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  // an exact path or name first (one ROM's path can contain another's), then
  // the first game containing the text
  for (const bool exact : {true, false}) {
    for (size_t i = 0; i < rom_infos_.size(); i++) {
      const auto &rom = rom_infos_[i];
      if (exact ? (rom.rom_path == text || rom.name == text)
                : (rom.name.find(text) != std::string::npos || rom.rom_path.find(text) != std::string::npos)) {
        // (a game started from the link must not leave the picker over the list)
        launcher_->close_picker();
        launcher_->select(i); // (lifts a filter that hides it)
        focused_rom_ = i;
        return true;
      }
    }
  }
  return false;
}

std::string Gui::settings_json() const {
  auto &emu = BoxEmu::get();
  const auto s = AppSettings::get().get_settings();
  std::string limits;
  for (const auto &l : kSettingLimits) {
    limits += fmt::format("{}\"{}\":[{},{}]", limits.empty() ? "" : ",", l.name, l.min, l.max);
  }
  return fmt::format("{{\"settings\":{{\"volume\":{},\"muted\":{},\"brightness\":{},\"video\":{},\"resume\":{},"
                     "\"haptic\":{}}},\"limits\":{{{}}}}}",
                     (int)std::lround(emu.volume()), emu.is_muted() ? 1 : 0, (int)std::lround(emu.brightness()),
                     (int)emu.video_setting(), s.resume_last_save ? 1 : 0, haptic_waveform_.load(), limits);
}

bool Gui::set_setting(const std::string &name, int value) {
  const auto *limit = std::find_if(std::begin(kSettingLimits), std::end(kSettingLimits),
                                   [&](const SettingLimit &l) { return name == l.name; });
  if (limit == std::end(kSettingLimits) || value < limit->min || value > limit->max) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  if (name == "volume") set_volume(value);
  else if (name == "muted") set_mute(value != 0);
  else if (name == "brightness") set_brightness(value);
  else if (name == "video") set_video_setting(value);
  else if (name == "resume") AppSettings::get().modify([value](Settings &s) { s.resume_last_save = value != 0; });
  else if (name == "haptic") set_haptic_waveform(value);
  launcher_->refresh_settings();
  return true;
}

Gui::Snapshot Gui::snapshot() {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  Snapshot shot;
  auto *screen = lv_screen_active();
  if (!screen) {
    return shot;
  }
  lv_obj_update_layout(screen);
  const int32_t w = lv_obj_get_width(screen), h = lv_obj_get_height(screen);
  const uint32_t stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565);
  shot.pixels.resize(stride * h);
  lv_draw_buf_t buf;
  if (lv_draw_buf_init(&buf, w, h, LV_COLOR_FORMAT_RGB565, stride, shot.pixels.data(), shot.pixels.size()) !=
          LV_RESULT_OK ||
      lv_snapshot_take_to_draw_buf(screen, LV_COLOR_FORMAT_RGB565, &buf) != LV_RESULT_OK) {
    shot.pixels.clear();
    return shot;
  }
  // (rows packed: the reader expects w * 2 bytes per row)
  if (stride != static_cast<uint32_t>(w) * 2) {
    for (int32_t y = 1; y < h; y++) {
      memmove(shot.pixels.data() + y * w * 2, shot.pixels.data() + y * stride, w * 2);
    }
    shot.pixels.resize(w * h * 2);
  }
  shot.width = w;
  shot.height = h;
  return shot;
}

// The selected game's boxart, in the decoder's buffer (the launcher asks for
// it after the console art, which shares that buffer while it is decoded).
const void *Gui::cover(size_t index) {
  if (index >= rom_infos_.size() || rom_infos_[index].boxart_path.empty()) return nullptr;
  decoder_.decode(rom_infos_[index].boxart_path.c_str());
  if (decoder_.get_size() <= 0) return nullptr;
  memset(&boxart_, 0, sizeof(boxart_));
  boxart_.header.magic = LV_IMAGE_HEADER_MAGIC;
  boxart_.header.cf = LV_COLOR_FORMAT_NATIVE;
  boxart_.header.w = decoder_.get_width();
  boxart_.header.h = decoder_.get_height();
  boxart_.data_size = decoder_.get_size();
  boxart_.data = decoder_.get_decoded_data();
  return &boxart_;
}

// boxart/consoles/<key>.jpg (at most about 120x18 to fit the status bar),
// decoded once and copied out of the decoder's buffer
const void *Gui::console_art(const std::string &key) {
  if (auto it = console_art_.find(key); it != console_art_.end()) {
    return it->second.data ? &it->second.desc : nullptr;
  }
  Art art;
  const std::string path = std::string(BoxEmu::mount_point) + "/boxart/consoles/" + key + ".jpg";
  std::error_code ec;
  if (std::filesystem::exists(path, ec)) {
    decoder_.decode(path.c_str());
    const size_t size = decoder_.get_size();
    if (size > 0) {
      art.data = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM));
      if (art.data) {
        memcpy(art.data, decoder_.get_decoded_data(), size);
        art.desc.header.magic = LV_IMAGE_HEADER_MAGIC;
        art.desc.header.cf = LV_COLOR_FORMAT_NATIVE;
        art.desc.header.w = decoder_.get_width();
        art.desc.header.h = decoder_.get_height();
        art.desc.data_size = size;
        art.desc.data = art.data;
      }
    }
  }
  auto &stored = console_art_[key] = art;
  return stored.data ? &stored.desc : nullptr;
}
