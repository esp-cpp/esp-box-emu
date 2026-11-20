#include "menu.hpp"

#include <cmath>
#include <span>

#include "battery_info.hpp"
#include "events.hpp"

namespace {
// a frame buffer: a screenshot is at most the panel's size
std::span<uint8_t> frame_storage(uint8_t *fb) {
  return {fb, BoxEmu::lcd_width() * BoxEmu::lcd_height() * 2};
}
} // namespace

Menu::Menu(const Config &config)
    : paused_image_path_(config.paused_image_path)
    , action_callback_(config.action_callback)
    , slot_image_callback_(config.slot_image_callback)
    , logger_({.tag = "Menu", .level = config.log_level}) {
  using K = emu_ui::Row::Kind;
  auto &emu = BoxEmu::get();
  std::vector<emu_ui::Row> settings;
  settings.push_back({.label = "Volume", .kind = K::SLIDER, .get = [&emu] { return (int)std::lround(emu.volume()); },
                      .set = [&emu](int v) { emu.volume(std::clamp(v, 0, 100)); }, .symbol = LV_SYMBOL_VOLUME_MAX});
  settings.push_back({.label = "Mute", .kind = K::TOGGLE, .get = [&emu] { return emu.is_muted() ? 1 : 0; },
                      .set = [&emu](int v) { emu.mute(v != 0); }, .symbol = LV_SYMBOL_MUTE});
  settings.push_back({.label = "Brightness", .kind = K::SLIDER, .min = 10,
                      .get = [&emu] { return (int)std::lround(emu.brightness()); },
                      .set = [&emu](int v) { emu.brightness((float)std::clamp(v, 10, 100)); },
                      .symbol = LV_SYMBOL_EYE_OPEN});
  settings.push_back({.label = "Video", .kind = K::CHOICE, .choices = {"Original", "Fit", "Fill"},
                      .get = [&emu] { return (int)emu.video_setting(); },
                      .set = [&emu](int v) { emu.video_setting(static_cast<VideoSetting>(v)); },
                      .symbol = LV_SYMBOL_IMAGE});
  {
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    // (the launcher's screen comes back when the menu goes)
    previous_screen_ = lv_screen_active();
    auto keypad = emu.keypad();
    view_ = std::make_unique<emu_ui::PauseMenu>(emu_ui::PauseMenu::Config{
        .metrics = emu_ui::Metrics::compact_320x240(),
        .keypad = keypad ? keypad->get_input_device() : nullptr,
        .slots = static_cast<int>(MAX_SLOT) + 1,
        .on_action = [this](emu_ui::PauseMenu::Action a) { on_action(a); },
        .on_slot =
            [this](int slot) {
              selected_slot_ = slot;
              update_slot_display();
            },
        .settings = std::move(settings),
    });
    view_->set_game(config.game_title, config.console_key);
  }
  using namespace std::placeholders;
  auto &events = espp::EventManager::get();
  events.add_subscriber(mute_button_topic, "menu", std::bind(&Menu::on_settings_changed, this, _1), 4 * 1024);
  events.add_subscriber(volume_changed_topic, "menu", std::bind(&Menu::on_settings_changed, this, _1), 4 * 1024);
  events.add_subscriber(battery_topic, "menu", std::bind(&Menu::on_battery, this, _1), 5 * 1024);
  logger_.info("Menu created");
}

Menu::~Menu() {
  {
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    auto &events = espp::EventManager::get();
    events.remove_subscriber(mute_button_topic, "menu");
    events.remove_subscriber(volume_changed_topic, "menu");
    events.remove_subscriber(battery_topic, "menu");
  }
  task_.stop();
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  if (previous_screen_ && view_ && lv_screen_active() == view_->screen()) lv_screen_load(previous_screen_);
  view_.reset();
}

void Menu::select_slot(size_t slot) {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  selected_slot_ = std::min(slot, MAX_SLOT);
  view_->set_slot(selected_slot_); // (calls on_slot when it changes)
  update_slot_display();
}

void Menu::pause() {
  paused_ = true;
  task_.stop();
  // wait out a timer callback already in lv_task_handler() (stop() does not)
  std::lock_guard<std::recursive_mutex> lk(mutex_);
}

void Menu::resume() {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  view_->set_slot(selected_slot_);
  update_slot_display();
  update_pause_image();
  view_->set_fps(get_fps());
  view_->refresh_settings();
  view_->show();
  // the game drew to the panel itself: repaint all of it
  lv_obj_invalidate(view_->screen());
  task_.periodic(16 * 1000);
  paused_ = false;
}

void Menu::on_action(emu_ui::PauseMenu::Action a) {
  // (the same order: RESUME, RESET, SAVE, LOAD, QUIT)
  const auto action = static_cast<Action>(a);
  action_callback_(action);
  if (action == Action::SAVE) {
    update_slot_display(); // the new screenshot
  } else if (action == Action::LOAD) {
    update_pause_image(); // the loaded state
  }
}

void Menu::update() {
  if (!paused_) {
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    lv_task_handler();
  }
}

void Menu::on_settings_changed(const std::vector<uint8_t> &) {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  view_->refresh_settings();
}

void Menu::on_battery(const std::vector<uint8_t> &data) {
  std::error_code ec;
  auto info = espp::deserialize<BatteryInfo>(data, ec);
  if (ec) return;
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  view_->set_battery((int)std::lround(info.level), info.charge_rate > 0.0f);
}

void Menu::update_pause_image() {
  const bool ok = emu_ui::load_screenshot(paused_image_path_.c_str(), frame_storage(BoxEmu::get().frame_buffer1()),
                                          pause_image_desc_);
  view_->set_pause_image(ok ? &pause_image_desc_ : nullptr);
}

void Menu::update_slot_display() {
  std::lock_guard<std::recursive_mutex> lk(mutex_);
  const auto path = slot_image_callback_ ? slot_image_callback_() : std::string{};
  const bool ok = !path.empty() && emu_ui::load_screenshot(path.c_str(), frame_storage(BoxEmu::get().frame_buffer0()),
                                                           slot_image_desc_);
  view_->set_slot_image(ok ? &slot_image_desc_ : nullptr);
}
