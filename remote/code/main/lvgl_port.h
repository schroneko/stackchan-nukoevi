#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#include "lvgl.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

void lvgl_port_init(void);
bool lvgl_port_lock(void);
void lvgl_port_unlock(void);

#ifdef __cplusplus
}
#endif

#endif  // LVGL_PORT_H