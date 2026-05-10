
#include "esp_now_init.h"

/**
 * @brief Initialize WiFi in STA mode and ESP-NOW with the specified channel
 * @param channel The WiFi channel to use (1-13)
 * @note This function initializes both WiFi subsystem in Station mode and ESP-NOW for communication
 * @details
 *      1. Initializes network interface
 *      2. Creates default event loop
 *      3. Initializes WiFi with default configuration
 *      4. Sets WiFi storage to RAM only
 *      5. Configures WiFi mode to Station (STA)
 *      6. Starts WiFi
 *      7. Sets the specified WiFi channel
 *      8. Optionally enables long range protocol if CONFIG_ESPNOW_ENABLE_LONG_RANGE is defined
 *      9. Configures ESP-NOW with forwarding disabled, 5 retry attempts, and receive disabled
 *      10. Initializes ESP-NOW with the configured parameters
 *      11. Reads and logs the device MAC address
 * @warning This function combines both WiFi and ESP-NOW initialization in a single call
 */
void wifi_espnow_init(uint8_t channel)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK(esp_wifi_set_protocol(
        ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
#endif
    espnow_config_t espnow_config = ESPNOW_INIT_CONFIG_DEFAULT();

    espnow_config.forward_enable         = false;
    espnow_config.forward_switch_channel = false;
    espnow_config.send_retry_num         = 5;
    espnow_config.receive_enable.forward = false;
    espnow_config.receive_enable.data    = false;

    espnow_init(&espnow_config);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_EFUSE_FACTORY);
    ESP_LOGI("espnow_init", "ESP-NOW initialized with MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);
}

/**
 * @brief Reinitialize WiFi and ESP-NOW with a new channel
 * @param new_channel The new WiFi channel to use (1-13)
 * @return uint8_t The actual channel that was set
 * @note This function properly deinitializes and reinitializes both WiFi and ESP-NOW
 * @details
 *      1. Checks if the new channel is different from current channel
 *      2. Deinitializes ESP-NOW
 *      3. Stops WiFi
 *      4. Restarts WiFi and sets the new channel
 *      5. Reinitializes ESP-NOW with the same configuration
 *      6. Logs the MAC address and current channel
 * @warning This function will temporarily interrupt ESP-NOW communication during reinitialization
 */
int wifi_espnow_reinit(uint8_t new_channel)
{
    uint8_t channel = 0;
    if (new_channel == channel) {
        ESP_LOGI("wifi reinit", "New Channel is same as current channel, no need to reinitialize");
        return channel;
    }
    // ESP_LOGI("reinit", "Reinitializing WiFi and ESP-NOW with channel: %d", new_channel);

    // 1. Deinitialize ESP-NOW
    espnow_deinit();

    // 2. Stop WiFi
    ESP_ERROR_CHECK(esp_wifi_stop());

    // 3. Restart WiFi and set new channel
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(new_channel, WIFI_SECOND_CHAN_NONE));

    // ESP_LOGI("reinit", "WiFi channel set to: %d", new_channel);

    // 4. Reinitialize ESP-NOW
    espnow_config_t espnow_config        = ESPNOW_INIT_CONFIG_DEFAULT();
    espnow_config.forward_enable         = false;
    espnow_config.forward_switch_channel = false;
    espnow_config.send_retry_num         = 5;
    espnow_config.receive_enable.forward = false;
    espnow_config.receive_enable.data    = false;

    espnow_init(&espnow_config);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_EFUSE_FACTORY);
    ESP_LOGI("reinit", "ESP-NOW reinitialized with MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
             mac[4], mac[5]);
    wifi_second_chan_t second;
    esp_wifi_get_channel(&channel, &second);
    ESP_LOGI("WiFi", "Current channel: %d", channel);
    return channel;
}

/**
 * @brief Send data packet via ESP-NOW broadcast
 * @param pkt Pointer to the data packet to send
 * @param len Length of the data packet in bytes
 * @note This function sends data using ESP-NOW broadcast address
 * @details
 *      1. Creates default ESP-NOW frame header
 *      2. Sends data using ESPNOW_DATA_TYPE_DATA type
 *      3. Uses broadcast address to send to all devices on the same channel
 *      4. Blocks until transmission completes (portMAX_DELAY timeout)
 */
void espnow_send_data(uint8_t *pkt, size_t len)
{
    espnow_frame_head_t frame_head = ESPNOW_FRAME_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(espnow_send(ESPNOW_DATA_TYPE_DATA, ESPNOW_ADDR_BROADCAST, pkt, len, &frame_head, portMAX_DELAY));
}