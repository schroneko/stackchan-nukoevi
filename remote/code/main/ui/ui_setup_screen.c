#include "ui_setup_screen.h"
#include "../lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

LV_IMG_DECLARE(updown_img);

lv_obj_t *setup_screen     = NULL;
lv_obj_t *channel_label    = NULL;
lv_obj_t *id_label         = NULL;
lv_obj_t *channel_dropdown = NULL;
lv_obj_t *id_dropdown      = NULL;

/**
 * @brief Create the setup screen UI with configuration options
 * @note This function creates a standalone screen with multiple UI elements:
 *       - Title label at the top
 *       - Channel selection dropdown with options 1-14
 *       - ID selection dropdown with options 0-50
 *       - Start button at the bottom for transitioning to running mode
 * @details The function sets up dropdown controls with initial selections and
 *          applies specific styling including background colors and transparency
 * @warning This function should only be called once per application run to avoid memory leaks
 */
void create_setup_screen()
{
    while (!lvgl_port_lock()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    lv_disp_t *disp = lv_disp_get_default();
    if (disp == NULL) {
        ESP_LOGE("UI", "No default display found!");
        lvgl_port_unlock();
        return;
    }

    if (setup_screen == NULL) {
        setup_screen = lv_obj_create(NULL);
    }

    lv_obj_clear_flag(setup_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Create title
    lv_obj_t *label = lv_label_create(setup_screen);
    lv_label_set_text(label, "StackChan :)");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);

    // Create Channel selection label
    channel_label = lv_label_create(setup_screen);
    lv_label_set_text(channel_label, "Channel:");
    lv_obj_align(channel_label, LV_ALIGN_TOP_LEFT, 5, 30);

    // Create Channel dropdown
    channel_dropdown = lv_dropdown_create(setup_screen);
    lv_dropdown_set_options(channel_dropdown, "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14");
    lv_dropdown_set_selected(channel_dropdown, 0);
    lv_obj_align(channel_dropdown, LV_ALIGN_TOP_LEFT, 5, 50);
    lv_dropdown_set_symbol(channel_dropdown, &updown_img);
    // Set dropdown background color
    lv_obj_set_style_bg_color(channel_dropdown, lv_color_make(255, 255, 255), LV_PART_MAIN);  // White
    lv_obj_set_style_bg_opa(channel_dropdown, LV_OPA_COVER, LV_PART_MAIN);  // Ensure background is opaque

    // Create ID selection label
    id_label = lv_label_create(setup_screen);
    lv_label_set_text(id_label, "Receiver ID:");
    lv_obj_align(id_label, LV_ALIGN_TOP_LEFT, 5, 100);

    // Create ID dropdown
    id_dropdown = lv_dropdown_create(setup_screen);
    lv_dropdown_set_options(
        id_dropdown,
        "0(Broadcast)"
        "\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n3"
        "0\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50");
    lv_dropdown_set_selected(id_dropdown, 0);
    lv_obj_align(id_dropdown, LV_ALIGN_TOP_LEFT, 5, 120);
    lv_dropdown_set_symbol(id_dropdown, &updown_img);
    // Set dropdown background color
    lv_obj_set_style_bg_color(id_dropdown, lv_color_make(255, 255, 255), LV_PART_MAIN);  // White
    lv_obj_set_style_bg_opa(id_dropdown, LV_OPA_COVER, LV_PART_MAIN);                    // Ensure background is opaque

    lv_obj_t *btn_label = lv_label_create(setup_screen);
    lv_label_set_text(btn_label, "Press to Start");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_18, 0);
    lv_obj_align(btn_label, LV_ALIGN_BOTTOM_MID, 0, -30);

    lv_obj_t *arrow_label = lv_label_create(setup_screen);
    lv_label_set_text(arrow_label, LV_SYMBOL_DOWN);
    lv_obj_align(arrow_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    lvgl_port_unlock();
}

/**
 * @brief Update the setup screen UI based on joystick input
 * @param data Pointer to joystick_data_t structure containing current joystick values and selection mode
 * @note This function handles joystick input to navigate and modify settings:
 *       - Highlights the currently selected dropdown (Channel or ID)
 *       - Increases/decreases values using joystick Y-axis movement
 *       - Updates the internal data structure with selected values
 * @details
 *      1. Changes background color of dropdowns to indicate selection
 *      2. Processes joystick Y-axis input for value modification
 *      3. Updates dropdown selections and corresponding data values
 *      4. Applies debouncing delay to prevent rapid value changes
 */
void update_setup_screen(joystick_data_t *data)
{
    while (!lvgl_port_lock()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // Update setup screen
    if (data->select_mode == CHANNEL_SELECT) {
        lv_obj_set_style_bg_color(channel_dropdown, lv_color_make(255, 255, 0), LV_PART_MAIN);  // Yellow
        lv_obj_set_style_bg_color(id_dropdown, lv_color_make(255, 255, 255), LV_PART_MAIN);     // White
    } else if (data->select_mode == ID_SELECT) {
        lv_obj_set_style_bg_color(channel_dropdown, lv_color_make(255, 255, 255), LV_PART_MAIN);  // White
        lv_obj_set_style_bg_color(id_dropdown, lv_color_make(255, 255, 0), LV_PART_MAIN);         // Yellow
    }
    // In setup mode, joystick up/down controls value increment/decrement
    if (data->joyY > Y_CENTER + DEAD_ZONE) {
        // Move up - Increase Channel
        if (data->select_mode == CHANNEL_SELECT) {
            uint16_t selected = lv_dropdown_get_selected(channel_dropdown);
            if (selected < 13) {  // Maximum index is 13 (corresponding to Channel 14)
                lv_dropdown_set_selected(channel_dropdown, selected + 1);
                data->channel = selected + 2;  // Index + 1 + 1 = displayed value
            }
        } else if (data->select_mode == ID_SELECT) {
            uint16_t selected = lv_dropdown_get_selected(id_dropdown);
            if (selected < 50) {
                lv_dropdown_set_selected(id_dropdown, selected + 1);
                data->id = selected + 1;
            }
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);  // Add delay to prevent rapid changes
    } else if (data->joyY < Y_CENTER - DEAD_ZONE) {
        // Move down - Decrease Channel
        if (data->select_mode == CHANNEL_SELECT) {
            uint16_t selected = lv_dropdown_get_selected(channel_dropdown);
            if (selected > 0) {
                lv_dropdown_set_selected(channel_dropdown, selected - 1);
                data->channel = selected;  // Index - 1 + 1 = index itself
            }
        } else if (data->select_mode == ID_SELECT) {
            uint16_t selected = lv_dropdown_get_selected(id_dropdown);
            if (selected > 0) {
                lv_dropdown_set_selected(id_dropdown, selected - 1);
                data->id = selected - 1;
            }
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);  // Add delay to prevent rapid changes
    }
    lvgl_port_unlock();
}

/**
 * @brief Destroy the setup screen and reset all UI object pointers to NULL
 * @note This function properly deletes the LVGL objects and resets internal pointers
 * @details
 *      1. Deletes the setup screen and all child objects using lv_obj_del
 *      2. Sets all UI object pointers to NULL to prevent dangling references
 */
void ui_setup_screen_destory()
{
    while (!lvgl_port_lock()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (setup_screen != NULL) {
        lv_obj_del(setup_screen);
        setup_screen = NULL;
    }
    lvgl_port_unlock();

    channel_label    = NULL;
    id_label         = NULL;
    channel_dropdown = NULL;
    id_dropdown      = NULL;
}