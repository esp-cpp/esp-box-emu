// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.1.1
// LVGL VERSION: 8.3.3
// PROJECT: emu


#include "ui.h"
#include "ui_helpers.h"
#include "ui_comp.h"

uint32_t LV_EVENT_GET_COMP_CHILD;

typedef struct {
uint32_t child_idx;
lv_obj_t* child;
} ui_comp_get_child_t;

lv_obj_t * ui_comp_get_child(lv_obj_t *comp, uint32_t child_idx) {
ui_comp_get_child_t info;
info.child = NULL;
info.child_idx = child_idx;
lv_event_send(comp, LV_EVENT_GET_COMP_CHILD, &info);
 return info.child;
}

void get_component_child_event_cb(lv_event_t* e) {
lv_obj_t** c = lv_event_get_user_data(e);
ui_comp_get_child_t* info = lv_event_get_param(e);
info->child = c[info->child_idx];
 }

void del_component_child_event_cb(lv_event_t* e) {
lv_obj_t** c = lv_event_get_user_data(e);
lv_mem_free(c); 
}


// COMPONENT rom

lv_obj_t *ui_rom_create(lv_obj_t *comp_parent) {

lv_obj_t *cui_rom;
cui_rom = lv_btn_create(comp_parent);
lv_obj_set_width( cui_rom, 150);
lv_obj_set_height( cui_rom, 150);
lv_obj_set_align( cui_rom, LV_ALIGN_CENTER );
lv_obj_add_flag( cui_rom, LV_OBJ_FLAG_SCROLL_ON_FOCUS );   /// Flags
lv_obj_clear_flag( cui_rom, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

lv_obj_t *cui_image;
cui_image = lv_img_create(cui_rom);
lv_obj_set_width( cui_image, 100);
lv_obj_set_height( cui_image, 100);
lv_obj_set_align( cui_image, LV_ALIGN_TOP_MID );
lv_obj_add_flag( cui_image, LV_OBJ_FLAG_ADV_HITTEST );   /// Flags
lv_obj_clear_flag( cui_image, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

lv_obj_t *cui_label;
cui_label = lv_label_create(cui_rom);
lv_obj_set_width( cui_label, lv_pct(100));
lv_obj_set_height( cui_label, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( cui_label, LV_ALIGN_BOTTOM_MID );
lv_obj_set_style_text_align(cui_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_t ** children = lv_mem_alloc(sizeof(lv_obj_t *) * _UI_COMP_ROM_NUM);
children[UI_COMP_ROM_ROM] = cui_rom;
children[UI_COMP_ROM_IMAGE] = cui_image;
children[UI_COMP_ROM_LABEL] = cui_label;
lv_obj_add_event_cb(cui_rom, get_component_child_event_cb, LV_EVENT_GET_COMP_CHILD, children);
lv_obj_add_event_cb(cui_rom, del_component_child_event_cb, LV_EVENT_DELETE, children);
return cui_rom; 
}

