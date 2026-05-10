#ifndef __JOYSTICK_HANDLE_H__
#define __JOYSTICK_HANDLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/i2c.h"
#include "i2c_bus.h"
#include "hal/i2c_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "../esp_now/esp_now_init.h"
#include "../ui/ui_setup_screen.h"
#include "../ui/ui_running_screen.h"
#include "../ui/ui_imu_screen.h"
#include "joystick_basic.h"

joystick_data_t joystick_init();
void handle_setup_screen(void *pvParam);
void handle_running_screen(void *pvParam);
void handle_imu_screen(void *pvParam);

#ifdef __cplusplus
}
#endif

#endif