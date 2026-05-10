#ifndef UI_IMU_SCREEN_H
#define UI_IMU_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <math.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_heap_caps.h"

#define MODE_IMU (2)

typedef struct {
    float pitch;
    float roll;
} IMU_Angle_t;

extern lv_obj_t *imu_screen;
extern lv_obj_t *imu_battery_label;
extern lv_obj_t *imu_channel_info_label;
extern lv_obj_t *imu_id_info_label;
extern lv_obj_t *imu_canvas;
extern lv_obj_t *imu_data_label;

void create_imu_screen(void);
IMU_Angle_t update_imu_screen(float ax, float ay, float az, uint8_t channel, uint8_t id, uint8_t bat);
void update_imu_cube(float ax, float ay, float az);
void ui_imu_screen_destory(void);

#ifdef __cplusplus
}
#endif

#endif