#include "menu.hpp"

extern "C" {
#include "ui.h"
}

void Menu::init_ui() {
  menu_ui_init();
}

void Menu::deinit_ui() {
  lv_obj_del(ui_menu_panel);
}
