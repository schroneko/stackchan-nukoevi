#ifndef _UI_RUNNING_SCREEN_H_
#define _UI_RUNNING_SCREEN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "../joystick/joystick_basic.h"
#include <math.h>
#include <esp_log.h>

#define MODE_RUNNING (1)

#define map(x, in_min, in_max, out_min, out_max) ((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min)

#define DEAD_ZONE (300)
#define X_CENTER  (2180)
#define Y_CENTER  (1960)
#define X_MIN     (630)
#define X_MAX     (3730)
#define Y_MIN     (310)
#define Y_MAX     (3460)

extern lv_obj_t* running_screen;
extern lv_obj_t* joystick_dot;
extern lv_obj_t* joystick_area;
extern lv_obj_t* battery_label;
extern lv_obj_t* channel_info_label;
extern lv_obj_t* id_info_label;

void create_running_screen();
void update_running_screen(int16_t joyX, int16_t joyY, uint8_t channel, uint8_t id, uint8_t bat);
void ui_running_screen_destory();

#ifdef __cplusplus
}
#endif

#endif  // _UI_RUNNING_SCREEN_H_
