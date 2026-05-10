#include "joystick_handle.h"

i2c_bus_device_handle_t i2c_device1;  // i2c device handle

/**
 * @brief Initialize joystick via I2C interface
 * @note This is an internal static function that configures I2C_NUM_0 as master with SDA on GPIO0 and SCL on GPIO26
 * @details
 *      1. Configures I2C master mode with 100kHz clock speed
 *      2. Creates I2C bus handle using I2C_NUM_0
 *      3. Scans the I2C bus to detect connected devices and logs their addresses
 *      4. Creates device handle for joystick at I2C address 0x54
 *      5. Assigns the device handle to global variable [i2c_device1]
 * @warning This function assumes the joystick device is at I2C address 0x54
 */
static void joystick_i2c_init()
{
    i2c_config_t conf;
    {
        conf.mode             = I2C_MODE_MASTER;
        conf.sda_io_num       = 0;
        conf.scl_io_num       = 26;
        conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = 100000;
        conf.clk_flags        = 0;
    };
    i2c_bus_handle_t i2c0_bus = i2c_bus_create(I2C_NUM_0, &conf);
    uint8_t buf[128];
    memset(buf, 0, sizeof(buf));

    i2c_bus_scan(i2c0_bus, buf, sizeof(buf));
    for (size_t i = 0; i < sizeof(buf); i++) {
        if (buf[i] != 0) {
            ESP_LOGI("I2C Scanner", "Found device at address 0x%02X", buf[i]);
        }
    }

    i2c_device1 = i2c_bus_device_create(i2c0_bus, 0x54, 0);
}

/**
 * @brief Read X and Y axis values from the joystick via I2C
 * @param joyX Pointer to store X-axis value (16-bit unsigned integer)
 * @param joyY Pointer to store Y-axis value (16-bit unsigned integer)
 * @return void
 * @details
 *      1. Reads 2 bytes from register address 0x00 (X-axis low/high bytes)
 *      2. Waits 10ms to ensure data stability
 *      3. Reads 2 bytes from register address 0x02 (Y-axis low/high bytes)
 *      4. Combines high and low bytes for both X and Y axes using bit shifting
 *      5. Stores the combined values in the provided pointers
 * @warning This function assumes the joystick provides 16-bit data in little-endian format
 */
static void joystick_read_xy(uint16_t *joyX, uint16_t *joyY)
{
    uint8_t data[4];
    esp_err_t ret = i2c_bus_read_bytes(i2c_device1, 0x00, 2, data);
    vTaskDelay(20 / portTICK_PERIOD_MS);
    ret |= i2c_bus_read_bytes(i2c_device1, 0x02, 2, &data[2]);
    if (ret == ESP_OK) {
        *joyX = (data[1] << 8) | data[0];
        *joyY = (data[3] << 8) | data[2];
    } else {
        // ESP_LOGE("I2C Joystick", "Failed to read joystick data");
    }
}

/**
 * @brief Public interface to initialize the joystick and return default configuration
 * @return joystick_data_t Structure containing initialized joystick parameters
 * @note This is the main initialization function exposed to users
 * @details
 *      1. Calls internal： device_joystick_init()
 *      2. Initializes all fields of 'joystick_data_t'
 *         - channel: 1 (default communication channel)
 *         - id: 0 (default target ID)
 *         - bat: 0 (battery level, to be updated later)
 *         - joyX, joyY: 0 (initial joystick positions)
 *         - screen_mode: MODE_SETUP (start in setup mode)
 *         - select_mode: CHANNEL_SELECT (default selection mode)
 * @return joystick_data_t
 */
joystick_data_t joystick_init()
{
    joystick_i2c_init();
    joystick_data_t tmp;
    tmp.channel     = 1;
    tmp.id          = 0;
    tmp.bat         = 0;
    tmp.joyX        = 0;
    tmp.joyY        = 0;
    tmp.accel_x     = 0.0f;
    tmp.accel_y     = 0.0f;
    tmp.accel_z     = 0.0f;
    tmp.screen_mode = MODE_SETUP;
    tmp.select_mode = CHANNEL_SELECT;
    tmp.btnB_status = false;
    return tmp;
}

/**
 * @brief Task to handle joystick setup screen
 * @param pvParam Pointer to joystick data, pointing to joystick_data_t structure
 * @note This function runs an infinite loop that continuously reads joystick XY coordinates
 *       and updates the setup screen when the screen mode is MODE_SETUP
 * @details Reads raw joystick data and then calls update_setup_screen function to update screen display
 *          Each loop iteration has a 50ms delay to ensure interface responsiveness
 */
void handle_setup_screen(void *pvParam)
{
    joystick_data_t *joystick_data = (joystick_data_t *)pvParam;
    while (1) {
        if (joystick_data->screen_mode == MODE_SETUP) {
            joystick_read_xy(&joystick_data->joyX, &joystick_data->joyY);
            update_setup_screen(joystick_data);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        } else {
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }
    }
}

/**
 * @brief Task to handle joystick running screen, responsible for reading joystick data and sending ESP-NOW control
 * packets
 * @param pvParam Pointer to joystick data, pointing to joystick_data_t structure
 * @note This function runs an infinite loop that reads joystick input, processes data, and sends control packets in
 * running mode
 * @details
 *      1. Reads raw X/Y values from the joystick via I2C
 *      2. Updates the running screen display with current values
 *      3. Applies deadzone correction to center the joystick values
 *      4. Maps raw values to yaw/pitch angle ranges (-1280 to 1280 for yaw, 0 to 900 for pitch)
 *      5. Only sends data when changes exceed threshold (5 units) to reduce network traffic
 *      6. Constructs and sends ESP-NOW packet containing target ID, yaw, pitch, speed and button status
 *      7. Each loop iteration has a 30ms delay when in running mode
 */
void handle_running_screen(void *pvParam)
{
    joystick_data_t *joystick_data = (joystick_data_t *)pvParam;
    // communicate packet
    uint8_t pkt[8];
    pkt[0]              = joystick_data->id;  // id: 0 for broadcast
    int16_t yaw_angle   = 0;
    int16_t pitch_angle = 0;
    int16_t last_yaw    = 0;
    int16_t last_pitch  = 0;
    int16_t speed_val   = 600;

    while (1) {
        // update screen and send packet when in running mode
        if (joystick_data->screen_mode == MODE_RUNNING) {
            joystick_read_xy(&joystick_data->joyX, &joystick_data->joyY);

            if (running_screen != NULL && lv_obj_is_valid(running_screen)) {
                update_running_screen(joystick_data->joyX, joystick_data->joyY, joystick_data->channel,
                                      joystick_data->id, joystick_data->bat);
            }

            // handle data from joystick
            if ((joystick_data->joyX < X_CENTER + DEAD_ZONE) && (joystick_data->joyX > X_CENTER - DEAD_ZONE)) {
                joystick_data->joyX = X_CENTER;
            }
            if ((joystick_data->joyY < Y_CENTER + DEAD_ZONE) && (joystick_data->joyY > Y_CENTER - DEAD_ZONE)) {
                joystick_data->joyY = Y_CENTER;
            }

            yaw_angle   = (int16_t)map(joystick_data->joyX, X_MIN, X_MAX, 1280, -1280);
            pitch_angle = (int16_t)map(joystick_data->joyY, Y_MIN, Y_MAX, 0, 900);

            // send pitch_angle and yaw_angle only when changes exceed threshold
            if (abs(yaw_angle - last_yaw) < 5 && abs(pitch_angle - last_pitch) < 5) {
                if (pkt[7] != joystick_data->btnB_status) {
                    pkt[7] = joystick_data->btnB_status;
                    espnow_send_data(pkt, sizeof(pkt));
                }
                vTaskDelay(30 / portTICK_PERIOD_MS);
                continue;
            }

            pkt[0] = joystick_data->id;
            memcpy(&pkt[1], &yaw_angle, sizeof(int16_t));
            memcpy(&pkt[3], &pitch_angle, sizeof(int16_t));
            memcpy(&pkt[5], &speed_val, sizeof(int16_t));
            pkt[7] = joystick_data->btnB_status;

#if 0 
            ESP_LOGI("handle_running_screen", "Yaw: %d, Pitch: %d, Speed: %d, id: %u, Button: %u", 
                        yaw_angle, pitch_angle, speed_val, joystick_data->id, joystick_data->btnB_status);
#endif

            last_yaw   = yaw_angle;
            last_pitch = pitch_angle;
            espnow_send_data(pkt, sizeof(pkt));
            vTaskDelay(30 / portTICK_PERIOD_MS);
        } else {
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }
    }
}

/**
 * @brief Task to handle joystick IMU screen functionality, processing IMU sensor data and sending ESP-NOW control
 * packets
 *
 * This function runs an infinite loop that reads IMU sensor data (accelerometer and gyroscope),
 * updates the IMU visualization screen, processes the angle data to control remote devices,
 * and sends ESP-NOW packets with the processed control information.
 * The function maps the IMU pitch and roll angles to yaw and pitch values for remote control,
 * applies filtering to reduce unnecessary transmissions, and sends control packets at regular intervals.
 *
 * @param pvParam Pointer to joystick data structure containing IMU sensor values, battery level,
 *                device ID, communication channel, and other control parameters
 * @details
 *      1. Continuously reads IMU data (acceleration and gyro values) from the joystick_data structure
 *      2. Updates the IMU screen visualization with current sensor values
 *      3. Limits roll values to range [-1.5, 1.5] and pitch values to range [0, 1.5]
 *      4. Maps limited angle values to appropriate yaw/pitch ranges for remote control (-1280 to 1280 for yaw, 900 to 0
 * for pitch)
 *      5. Only sends control packets when changes exceed threshold (10 units) to minimize network traffic
 *      6. Constructs and transmits ESP-NOW packet with device ID, yaw, pitch, speed, and button status
 *      7. Includes a 50ms delay between iterations when in IMU mode, 200ms otherwise
 */
void handle_imu_screen(void *pvParam)
{
    joystick_data_t *joystick_data = (joystick_data_t *)pvParam;

    static IMU_Angle_t last_imu_angle = {0.0f, 0.0f};

    // communicate packet
    uint8_t pkt[8];
    pkt[0]              = joystick_data->id;  // id: 0 for broadcast
    int16_t yaw_angle   = 0;
    int16_t pitch_angle = 0;
    int16_t last_yaw    = 0;
    int16_t last_pitch  = 0;
    int16_t speed_val   = 600;

    while (1) {
        // update screen and send packet when in running mode
        if (joystick_data->screen_mode == MODE_IMU) {
            IMU_Angle_t imu_angle =
                update_imu_screen(joystick_data->accel_x, joystick_data->accel_y, joystick_data->accel_z,
                                  joystick_data->bat, joystick_data->id, joystick_data->channel);

            // Limit the roll value to the range of -1.5 to 1.5
            float limited_roll = fmaxf(-1.5f, fminf(1.5f, imu_angle.roll));
            // Limit the pitch value to the range of 0 to 1.5
            float limited_pitch = fmaxf(0.0f, fminf(1.5f, imu_angle.pitch));

            yaw_angle   = (int16_t)map(limited_roll, -1.5, 1.5, -1280, 1280);
            pitch_angle = (int16_t)map(limited_pitch, 0, 1.5, 900, 0);

            if (abs(yaw_angle - last_yaw) < 10 && abs(last_pitch - pitch_angle) < 10) {
                vTaskDelay(30 / portTICK_PERIOD_MS);
                continue;
            }
            last_yaw   = yaw_angle;
            last_pitch = pitch_angle;

            pkt[0] = joystick_data->id;
            memcpy(&pkt[1], &yaw_angle, sizeof(int16_t));
            memcpy(&pkt[3], &pitch_angle, sizeof(int16_t));
            memcpy(&pkt[5], &speed_val, sizeof(int16_t));
            pkt[7] = joystick_data->btnB_status;
            espnow_send_data(pkt, sizeof(pkt));

#if 0 
            // ESP_LOGI("handle_imu_screen", "yaw_angle: %.2f, pitch_angle:%.2f, yaw: %d, pitch: %d\n", 
                                            imu_angle.roll, imu_angle.pitch, yaw_angle, pitch_angle);
#endif

            vTaskDelay(30 / portTICK_PERIOD_MS);
        } else {
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }
    }
}