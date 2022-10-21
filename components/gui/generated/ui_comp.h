// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.1.1
// LVGL VERSION: 8.3.3
// PROJECT: emu

#ifndef _EMU_UI_COMP_H
#define _EMU_UI_COMP_H

#include "ui.h"

lv_obj_t * ui_comp_get_child(lv_obj_t *comp, uint32_t child_idx);
extern uint32_t LV_EVENT_GET_COMP_CHILD;

// COMPONENT rom
#define UI_COMP_ROM_ROM 0
#define UI_COMP_ROM_IMAGE 1
#define UI_COMP_ROM_LABEL 2
#define _UI_COMP_ROM_NUM 3
lv_obj_t *ui_rom_create(lv_obj_t *comp_parent);

#endif
