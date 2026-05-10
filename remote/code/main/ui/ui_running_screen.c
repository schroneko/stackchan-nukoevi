#include "ui_running_screen.h"
#include "../lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

lv_obj_t* running_screen     = NULL;
lv_obj_t* joystick_dot       = NULL;
lv_obj_t* joystick_area      = NULL;
lv_obj_t* battery_label      = NULL;
lv_obj_t* channel_info_label = NULL;
lv_obj_t* id_info_label      = NULL;

/**
 * @brief Create the running screen UI with joystick visualization and status information
 * @note This function creates a standalone screen with multiple UI elements:
 *       - Title label at the top
 *       - Joystick visualization area with crosshair
 *       - Red dot representing joystick position
 *       - Battery level display
 *       - Channel information display
 *       - Device ID information display
 * @details The function creates a 115x115 pixel joystick area with crosshair lines
 *          and a red circular dot that represents the current joystick position
 * @warning This function should only be called once per application run to avoid memory leaks
 */
void create_running_screen()
{
    while (!lvgl_port_lock()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    lv_disp_t* disp = lv_disp_get_default();
    if (disp == NULL) {
        ESP_LOGE("UI", "No default display found!");
        lvgl_port_unlock();
        return;
    }

    if (running_screen == NULL) {
        running_screen = lv_obj_create(NULL);
    }

    if (running_screen == NULL) {
        ESP_LOGE("UI", "Failed to create running screen!");
        return;
    }

    lv_obj_clear_flag(running_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Create title
    lv_obj_t* label = lv_label_create(running_screen);

    lv_label_set_text(label, "StackChan :)");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);

    // Create joystick area
    joystick_area = lv_obj_create(running_screen);
    lv_obj_set_size(joystick_area, 115, 115);  // Reduced size
    lv_obj_align(joystick_area, LV_ALIGN_TOP_MID, 0, 35);

    // Set joystick area style
    lv_obj_set_style_border_width(joystick_area, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(joystick_area, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(joystick_area, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(joystick_area, 0, LV_PART_MAIN);
    lv_obj_clear_flag(joystick_area, LV_OBJ_FLAG_SCROLLABLE);

    // Add horizontal crosshair line
    lv_obj_t* cross_line_h        = lv_line_create(joystick_area);
    static lv_point_t points_h[2] = {{0, 57}, {115, 57}};  // Horizontal center line
    lv_line_set_points(cross_line_h, points_h, 2);
    lv_obj_set_style_line_color(cross_line_h, lv_color_make(64, 64, 64), 0);
    lv_obj_set_style_line_width(cross_line_h, 1, 0);

    // Add vertical crosshair line
    lv_obj_t* cross_line_v        = lv_line_create(joystick_area);
    static lv_point_t points_v[2] = {{56, 0}, {56, 115}};  // Vertical center line
    lv_line_set_points(cross_line_v, points_v, 2);
    lv_obj_set_style_line_color(cross_line_v, lv_color_make(64, 64, 64), 0);
    lv_obj_set_style_line_width(cross_line_v, 1, 0);

    // Create joystick dot
    joystick_dot = lv_obj_create(joystick_area);
    lv_obj_set_size(joystick_dot, 10, 10);                                  // Dot size
    lv_obj_set_style_radius(joystick_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);  // Set to circle
    lv_obj_set_style_bg_color(joystick_dot, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_set_style_border_width(joystick_dot, 0, LV_PART_MAIN);
    lv_obj_align(joystick_dot, LV_ALIGN_CENTER, 0, 0);

    // Create battery level display
    battery_label = lv_label_create(running_screen);
    lv_label_set_text(battery_label, "Bat: 100%%");
    lv_obj_align(battery_label, LV_ALIGN_TOP_LEFT, 10, 160);
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_14, 0);

    // Create Channel information display
    channel_info_label = lv_label_create(running_screen);
    lv_label_set_text(channel_info_label, "Channel: 1");
    lv_obj_align(channel_info_label, LV_ALIGN_TOP_LEFT, 10, 180);
    lv_obj_set_style_text_font(channel_info_label, &lv_font_montserrat_14, 0);

    // Create ID information display
    id_info_label = lv_label_create(running_screen);
    lv_label_set_text(id_info_label, "Receiver ID: \n0(broadcast)");
    lv_obj_align(id_info_label, LV_ALIGN_TOP_LEFT, 10, 200);
    lv_obj_set_style_text_font(id_info_label, &lv_font_montserrat_14, 0);

    lvgl_port_unlock();
}

/**
 * @brief Update the running screen UI with current joystick values and status information
 * @param joyX X-axis value from joystick (raw value to be mapped to screen coordinates)
 * @param joyY Y-axis value from joystick (raw value to be mapped to screen coordinates)
 * @param channel Current WiFi channel being used
 * @param id Device ID for communication
 * @param bat Battery level percentage (0-100)
 * @note This function maps joystick values to the 115x115 pixel joystick area
 *       and applies deadzone correction to center the dot when joystick is near center position
 * @details
 *      1. Maps raw joystick values to screen coordinates within the joystick area
 *      2. Applies deadzone correction to keep dot centered when joystick is in neutral position
 *      3. Clamps values to prevent the dot from going outside the joystick area
 *      4. Updates the position of the joystick dot
 *      5. Updates battery level, channel and ID information labels
 */
void update_running_screen(int16_t joyX, int16_t joyY, uint8_t channel, uint8_t id, uint8_t bat)
{
    // Map joystick values to 115x115 area (using your mapping approach)
    int16_t x_pos = map(joyX, X_MIN, X_MAX, 5, 110);  // Leave 5px margin
    int16_t y_pos = map(joyY, Y_MIN, Y_MAX, 110, 5);  // Y-axis inverted

    // Apply deadzone
    int16_t x_center = map(X_CENTER, X_MIN, X_MAX, 5, 110);
    int16_t y_center = map(Y_CENTER, Y_MIN, Y_MAX, 110, 5);

    if (abs(joyX - X_CENTER) < DEAD_ZONE) {
        x_pos = x_center;
    }
    if (abs(joyY - Y_CENTER) < DEAD_ZONE) {
        y_pos = y_center;
    }

    // Limit range
    x_pos = fmax(5, fmin(x_pos, 110));
    y_pos = fmax(5, fmin(y_pos, 110));

    while (!lvgl_port_lock()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // Update joystick dot position (relative to joystick_area center)
    lv_obj_align(joystick_dot, LV_ALIGN_TOP_LEFT, x_pos - 5, y_pos - 5);

    // Update battery level
    lv_label_set_text_fmt(battery_label, "Bat: %d%%", bat);

    // Update Channel and ID display
    lv_label_set_text_fmt(channel_info_label, "Channel: %u", channel);
    if (id == 0) {
        lv_label_set_text(id_info_label, "Receiver ID:\n  0(broadcast)");
    } else {
        lv_label_set_text_fmt(id_info_label, "Receiver ID: %u", id);
    }
    lvgl_port_unlock();
}

/**
 * @brief Reset all UI object pointers to NULL to prepare for screen destruction
 * @note This function does not actually destroy the UI objects, but resets the pointers
 *       that reference them, allowing the UI to be recreated or switched
 * @warning The actual UI objects should be destroyed separately using LVGL's object destruction functions
 */
void ui_running_screen_destory()
{
    while (!lvgl_port_lock()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (running_screen != NULL) {
        lv_obj_del(running_screen);
        running_screen = NULL;
    }
    lvgl_port_unlock();
    joystick_dot       = NULL;
    joystick_area      = NULL;
    battery_label      = NULL;
    channel_info_label = NULL;
    id_info_label      = NULL;
}