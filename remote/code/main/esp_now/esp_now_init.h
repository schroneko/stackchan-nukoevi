#ifndef __ESP_NOW_INIT_H__
#define __ESP_NOW_INIT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <esp_mac.h>
#include <espnow.h>
#include <espnow_storage.h>
#include <espnow_utils.h>
#include "esp_wifi.h"

void wifi_espnow_init(uint8_t channel);
int wifi_espnow_reinit(uint8_t new_channel);
void espnow_send_data(uint8_t *pkt, size_t len);

#ifdef __cplusplus
}
#endif

#endif