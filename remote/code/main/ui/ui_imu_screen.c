#include "ui_imu_screen.h"
#include <math.h>
#include <stdio.h>
#include "../lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static lv_obj_t *edge_lines[12];
static lv_obj_t *cross_lines[2];  // 标识用的对角线

static lv_point_t edge_points[12][2];  // 12条边，每条2个点
static lv_point_t cross_points[2][2];  // 2条对角线，每条2个点

lv_obj_t *imu_screen;
lv_obj_t *cube_container = NULL;
lv_obj_t *imu_battery_label;
lv_obj_t *imu_channel_info_label;
lv_obj_t *imu_id_info_label;
lv_obj_t *imu_data_label;

// 3D cube parameters
typedef struct {
    float x, y, z;
} Point3D;

typedef struct {
    float x, y;
} Point2D;

// Define the 8 vertices of a cube
static Point3D vertices[8] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
                              {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}};

// Define the 12 edges of a cube (indices of vertices connected)
static int edges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},  // fornt
    {4, 5}, {5, 6}, {6, 7}, {7, 4},  // behind
    {0, 4}, {1, 5}, {2, 6}, {3, 7}   // middle connecting line
};

IMU_Angle_t g_imu_angle = {0.0f, 0.0f};

/**
 * @brief Creates the IMU screen with all UI elements including a 3D cube visualization
 *
 * This function initializes and creates the main IMU screen interface with:
 * - Title label showing "StackChan :)"
 * - A cube container for 3D visualization
 * - 12 cube edge lines forming a 3D cube
 * - 2 diagonal cross lines for orientation reference
 * - Battery status label
 * - Channel information label
 * - Receiver ID label
 *
 * The function handles LVGL locking to ensure thread-safe operations.
 */
void create_imu_screen()
{
    while (!lvgl_port_lock()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (imu_screen == NULL) {
        imu_screen = lv_obj_create(NULL);
    }
    lv_obj_clear_flag(imu_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Create title
    lv_obj_t *label = lv_label_create(imu_screen);
    lv_label_set_text(label, "StackChan :)");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);

    // create container
    cube_container = lv_obj_create(imu_screen);
    lv_obj_set_size(cube_container, 115, 115);
    lv_obj_align(cube_container, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_style_bg_opa(cube_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cube_container, 0, 0);
    lv_obj_set_style_pad_all(cube_container, 0, 0);
    lv_obj_clear_flag(cube_container, LV_OBJ_FLAG_SCROLLABLE);

    // Create 12 cube edges
    for (int i = 0; i < 12; i++) {
        edge_lines[i] = lv_line_create(cube_container);
        lv_obj_set_style_line_width(edge_lines[i], 2, 0);
        lv_obj_set_style_line_color(edge_lines[i], lv_color_black(), 0);
        lv_obj_add_flag(edge_lines[i], LV_OBJ_FLAG_FLOATING);

        // Add default coordinate points to avoid empty line segments
        lv_point_t default_points[2] = {{0, 0}, {0, 0}};
        lv_line_set_points(edge_lines[i], default_points, 2);
    }

    // Create 2 lines identifying the diagonals
    for (int i = 0; i < 2; i++) {
        cross_lines[i] = lv_line_create(cube_container);
        lv_obj_set_style_line_width(cross_lines[i], 2, 0);
        lv_obj_set_style_line_color(cross_lines[i], lv_color_make(0, 255, 255), 0);
        lv_obj_add_flag(cross_lines[i], LV_OBJ_FLAG_FLOATING);

        // Add default coordinate
        lv_point_t default_cross_points[2] = {{0, 0}, {0, 0}};
        lv_line_set_points(cross_lines[i], default_cross_points, 2);
    }

    // Create other UI elements
    imu_battery_label = lv_label_create(imu_screen);
    lv_label_set_text(imu_battery_label, "Bat: 100%");
    lv_obj_align(imu_battery_label, LV_ALIGN_TOP_LEFT, 10, 160);
    lv_obj_set_style_text_font(imu_battery_label, &lv_font_montserrat_14, 0);

    imu_channel_info_label = lv_label_create(imu_screen);
    lv_label_set_text(imu_channel_info_label, "Channel: 1");
    lv_obj_align(imu_channel_info_label, LV_ALIGN_TOP_LEFT, 10, 180);
    lv_obj_set_style_text_font(imu_channel_info_label, &lv_font_montserrat_14, 0);

    imu_id_info_label = lv_label_create(imu_screen);
    lv_label_set_text(imu_id_info_label, "Receiver ID:\n0(broadcast)");
    lv_obj_align(imu_id_info_label, LV_ALIGN_TOP_LEFT, 10, 200);
    lv_obj_set_style_text_font(imu_id_info_label, &lv_font_montserrat_14, 0);

    lvgl_port_unlock();
}

/**
 * @brief Updates the 3D cube visualization based on IMU accelerometer data
 *
 * This function takes accelerometer readings (ax, ay, az) and calculates
 * the pitch and roll angles to rotate a 3D cube representation. It performs:
 * - Input validation to check for NaN or infinite values
 * - Calculation of pitch and roll angles using trigonometric functions
 * - 3D to 2D projection of cube vertices with rotation transformations
 * - Updates all 12 cube edge lines and 2 diagonal marker lines
 *
 * @param ax Accelerometer X-axis reading
 * @param ay Accelerometer Y-axis reading
 * @param az Accelerometer Z-axis reading
 */
void update_imu_cube(float ax, float ay, float az)
{
    // Check if the input value is valid
    if (isnan(ax) || isnan(ay) || isnan(az) || isinf(ax) || isinf(ay) || isinf(az)) {
        printf("Invalid IMU data received!\n");
        return;
    }

    // Calculate tilt angle (based on gravitational acceleration)
    float pitch = atan2(ay, sqrt(ax * ax + az * az));
    float roll  = atan2(ax, sqrt(ay * ay + az * az));

    if (az < 0) {
        pitch = M_PI - pitch;
    }

    g_imu_angle.pitch = pitch;
    g_imu_angle.roll  = roll;

    // 3D projection calculation
    Point2D projected[8];  // Store the 2D points after projection
    int centerX = lv_obj_get_width(cube_container) / 2;
    int centerY = lv_obj_get_height(cube_container) / 2;

    float scale = 30.0f;

    for (int i = 0; i < 8; i++) {
        Point3D p = vertices[i];

        // Pitch
        float y1 = p.y * cos(pitch) - p.z * sin(pitch);
        float z1 = p.y * sin(pitch) + p.z * cos(pitch);
        float x1 = p.x;

        // Roll
        float x2 = x1 * cos(roll) + z1 * sin(roll);
        float z2 = -x1 * sin(roll) + z1 * cos(roll);
        float y2 = y1;

        // Orthographic projection
        projected[i].x = centerX + x2 * scale;
        projected[i].y = centerY + y2 * scale;
    }

    // Update 12 cube edges
    for (int i = 0; i < 12; i++) {
        edge_points[i][0].x = (int16_t)projected[edges[i][0]].x;
        edge_points[i][0].y = (int16_t)projected[edges[i][0]].y;
        edge_points[i][1].x = (int16_t)projected[edges[i][1]].x;
        edge_points[i][1].y = (int16_t)projected[edges[i][1]].y;
        lv_line_set_points(edge_lines[i], edge_points[i], 2);
    }

    // Update 2 diagonal markers
    cross_points[0][0] = (lv_point_t){(int16_t)projected[0].x, (int16_t)projected[0].y};
    cross_points[0][1] = (lv_point_t){(int16_t)projected[2].x, (int16_t)projected[2].y};
    lv_line_set_points(cross_lines[0], cross_points[0], 2);

    cross_points[1][0] = (lv_point_t){(int16_t)projected[1].x, (int16_t)projected[1].y};
    cross_points[1][1] = (lv_point_t){(int16_t)projected[3].x, (int16_t)projected[3].y};
    lv_line_set_points(cross_lines[1], cross_points[1], 2);
}

/**
 * @brief Updates the complete IMU screen with sensor data and system information
 *
 * This function updates the entire IMU screen with real-time data including:
 * - Updates the 3D cube visualization via update_imu_cube()
 * - Battery percentage display
 * - Communication channel information
 * - Receiver ID display (handles broadcast case)
 *
 * Optimized to only update labels when values have changed using static tracking variables.
 *
 * @param ax Accelerometer X-axis reading
 * @param ay Accelerometer Y-axis reading
 * @param az Accelerometer Z-axis reading
 * @param bat Battery level percentage (0-100)
 * @param id Receiver ID (0 for broadcast)
 * @param channel ESP-NOW communication channel
 * @return IMU_Angle_t Current pitch and roll angles calculated from IMU data
 */
IMU_Angle_t update_imu_screen(float ax, float ay, float az, uint8_t bat, uint8_t id, uint8_t channel)
{
    static uint8_t last_bat     = 0xFF;
    static uint8_t last_id      = 0xFF;
    static uint8_t last_channel = 0xFF;

    while (!lvgl_port_lock()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    update_imu_cube(ax, ay, az);

    if (imu_battery_label && bat != last_bat) {
        lv_label_set_text_fmt(imu_battery_label, "Bat: %d%%", bat);
        last_bat = bat;
    }

    if (imu_channel_info_label && channel != last_channel) {
        lv_label_set_text_fmt(imu_channel_info_label, "Channel: %u", channel);
        last_channel = channel;
    }

    if (imu_id_info_label && id != last_id) {
        if (id == 0) {
            lv_label_set_text(imu_id_info_label, "Receiver ID:\n0(broadcast)");
        } else {
            lv_label_set_text_fmt(imu_id_info_label, "Receiver ID: %u", id);
        }
        last_id = id;
    }
    lvgl_port_unlock();
    return g_imu_angle;
}

/**
 * @brief Destroys and cleans up the IMU screen resources
 *
 * This function safely removes the IMU screen from memory by:
 * - Acquiring LVGL lock for thread safety
 * - Deleting the main screen object if it exists
 * - Setting all UI element pointers to NULL to prevent dangling references
 *
 * After execution, the screen will need to be recreated before use again.
 */
void ui_imu_screen_destory()
{
    while (!lvgl_port_lock()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (imu_screen != NULL) {
        lv_obj_del(imu_screen);
        imu_screen = NULL;
    }
    lvgl_port_unlock();

    imu_battery_label      = NULL;
    imu_channel_info_label = NULL;
    imu_id_info_label      = NULL;
    imu_data_label         = NULL;
}