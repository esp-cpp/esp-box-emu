// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.1.1
// LVGL VERSION: 8.3.3
// PROJECT: emu

#ifndef _EMU_UI_H
#define _EMU_UI_H

#ifdef __cplusplus
extern "C" {
#endif

    #include "lvgl/lvgl.h"

extern lv_obj_t *ui_romscreen;
extern lv_obj_t *ui_header;
void ui_event_settingsbutton( lv_event_t * e);
extern lv_obj_t *ui_settingsbutton;
extern lv_obj_t *ui_Screen1_Label2;
extern lv_obj_t *ui_Screen1_Label1;
extern lv_obj_t *ui_playbutton;
extern lv_obj_t *ui_Screen1_Label3;
extern lv_obj_t *ui_rompanel;
extern lv_obj_t *ui_boxartpanel;
extern lv_obj_t *ui_boxart;
extern lv_obj_t *ui_settingsscreen;
extern lv_obj_t *ui_header1;
void ui_event_closebutton( lv_event_t * e);
extern lv_obj_t *ui_closebutton;
extern lv_obj_t *ui_Screen1_Label4;
extern lv_obj_t *ui_Screen1_Label5;
extern lv_obj_t *ui_settingspanel;
extern lv_obj_t *ui_volumepanel;
extern lv_obj_t *ui_volumebar;
extern lv_obj_t *ui_mutebutton;
extern lv_obj_t *ui_settingsscreen_Label1;
extern lv_obj_t *ui_volumedownbutton;
extern lv_obj_t *ui_settingsscreen_Label2;
extern lv_obj_t *ui_volumeupbutton;
extern lv_obj_t *ui_settingsscreen_Label3;






void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
