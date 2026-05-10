#ifndef UI_H
#define UI_H

#include "esp_log.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui_setup_screen.h"
#include "ui_running_screen.h"
#include "ui_imu_screen.h"

void ui_init();
void switch_screen(int screen_id);

#endif
