#ifndef _UI_SETUP_SCREEN_H_
#define _UI_SETUP_SCREEN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_log.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "../joystick/joystick_basic.h"

#define MODE_SETUP (0)

#define CHANNEL_SELECT (0)
#define ID_SELECT      (1)

#define DEAD_ZONE (300)
#define X_CENTER  (2180)
#define Y_CENTER  (1960)
#define X_MIN     (630)
#define X_MAX     (3730)
#define Y_MIN     (310)
#define Y_MAX     (3460)

extern lv_obj_t *setup_screen;
extern lv_obj_t *channel_label;
extern lv_obj_t *id_label;
extern lv_obj_t *start_btn;
extern lv_obj_t *channel_dropdown;
extern lv_obj_t *id_dropdown;

void create_setup_screen();
void update_setup_screen(joystick_data_t *data);
void ui_setup_screen_destory();

#ifdef __cplusplus
}
#endif

#endif  // _UI_SETUP_SCREEN_H_
