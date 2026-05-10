#include "ui.h"

/**
 * @brief Switches between different UI screens based on the provided screen ID
 *
 * This function manages the display of different UI screens (setup, running, IMU)
 * by checking if the requested screen exists, creating it if necessary, validating
 * the screen object, and then loading it with a slide-left animation effect.
 *
 * The function implements error handling by destroying and recreating invalid screens
 * using goto statements for retry logic. Each screen type has its own creation
 * and validation flow.
 *
 * @param screen_id An integer representing the target screen mode:
 *                  - MODE_SETUP: Configuration/setup screen
 *                  - MODE_RUNNING: Main operational screen
 *                  - MODE_IMU: IMU data visualization screen
 *                  - Any other value: Logs an error message
 *
 * @note The function uses LVGL's animation API to provide smooth screen transitions
 *       with a 200ms left slide animation. Thread safety should be considered when
 *       calling this function from different tasks.
 *
 * @warning This function relies on external screen objects and creation/destruction
 *          functions that must be implemented in other UI modules. The use of goto
 *          statements may affect code maintainability.
 */
void switch_screen(int screen_id)
{
    if (screen_id == MODE_SETUP) {
    setup_create:
        if (setup_screen == NULL) {
            create_setup_screen();
            ESP_LOGI("UI", "Setup screen created");
        }
        // Load only if object is valid
        if (setup_screen != NULL && lv_obj_is_valid(setup_screen)) {
            ESP_LOGI("UI", "Setup screen loaded");
            lv_scr_load_anim(setup_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
        } else {
            ESP_LOGE("UI", "Setup screen is NULL or invalid!");
            ui_setup_screen_destory();
            goto setup_create;
        }
    } else if (screen_id == MODE_RUNNING) {
    running_create:
        if (running_screen == NULL) {
            create_running_screen();
            ESP_LOGI("UI", "Running screen created");
        }
        // Load only if object is valid
        if (running_screen != NULL && lv_obj_is_valid(running_screen)) {
            ESP_LOGI("UI", "Running screen loaded");
            lv_scr_load_anim(running_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
        } else {
            ESP_LOGE("UI", "Running screen is NULL or invalid!");
            ui_running_screen_destory();
            goto running_create;
        }
    } else if (screen_id == MODE_IMU) {
    imu_create:
        if (imu_screen == NULL) {
            create_imu_screen();
            ESP_LOGI("UI", "IMU screen created");
        }
        // Load only if object is valid
        if (imu_screen != NULL && lv_obj_is_valid(imu_screen)) {
            ESP_LOGI("UI", "IMU screen loaded");
            lv_scr_load_anim(imu_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
        } else {
            ESP_LOGE("UI", "Running screen is NULL or invalid!");
            ui_imu_screen_destory();
            goto imu_create;
        }
    } else {
        ESP_LOGE("UI", "Invalid screen mode!");
    }
}

/**
 * @brief Initialize the UI system by creating and loading the initial setup screen
 * @note This function serves as the entry point for UI initialization
 * @details
 *      1. Creates the setup screen using create_setup_screen()
 *      2. Immediately loads the setup screen as the current display
 *      3. Sets up the initial UI state for user interaction
 * @warning This function should only be called once during application startup
 */
void ui_init()
{
    create_setup_screen();
    lv_disp_load_scr(setup_screen);
}