#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "event_manager.hpp"
#include "high_resolution_timer.hpp"
#include "logger.hpp"

#include "emu-ui/pause_menu.hpp"

#include "box-emu.hpp"
#include "statistics.hpp"

/// In-game pause menu: resume / reset / save / load / quit, a save slot
/// selector with the slot's screenshot, and volume / brightness / video
/// scaling. The screen is emu-ui's (shared with tab5-emu).
class Menu {
public:
  static constexpr size_t MAX_SLOT = 5;
  enum class Action { RESUME, RESET, SAVE, LOAD, QUIT };

  typedef std::function<void(Action)> action_fn;
  typedef std::function<std::string()> slot_image_fn;

  struct Config {
    std::string paused_image_path;
    action_fn action_callback;
    slot_image_fn slot_image_callback;
    std::string game_title;  ///< in the menu's title bar
    std::string console_key; ///< rom_info's ConsoleInfo::key: the background
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  explicit Menu(const Config &config);
  ~Menu();

  size_t get_selected_slot() const { return selected_slot_; }
  void select_slot(size_t slot);
  /// Pick the slot without touching the (hidden) menu: no LVGL, and no
  /// preview read into frame_buffer0, which the running core may draw in.
  /// resume() shows it when the menu next opens.
  void set_selected_slot(size_t slot) { selected_slot_ = std::min(slot, MAX_SLOT); }

  bool is_paused() const { return paused_; }
  void pause();
  void resume();

protected:
  void update();
  void update_slot_display();
  void update_pause_image();
  void on_action(emu_ui::PauseMenu::Action action);
  void on_battery(const std::vector<uint8_t> &data);
  void on_settings_changed(const std::vector<uint8_t> &data);

  std::string paused_image_path_;
  action_fn action_callback_;
  slot_image_fn slot_image_callback_;
  std::unique_ptr<emu_ui::PauseMenu> view_;
  lv_obj_t *previous_screen_{nullptr};

  // the screenshots are read into the frame buffers (free while paused): no
  // room for 2 x 150 KB more in the box's heap
  lv_image_dsc_t pause_image_desc_{};
  lv_image_dsc_t slot_image_desc_{};

  std::atomic<size_t> selected_slot_{0};
  std::atomic<bool> paused_{true};
  espp::HighResolutionTimer task_{{
      .name = "Menu Task",
      .callback = std::bind(&Menu::update, this),
  }};
  espp::Logger logger_;
  std::recursive_mutex &mutex_{BoxEmu::lvgl_mutex()}; // shared with the GUI (one LVGL instance)
};
