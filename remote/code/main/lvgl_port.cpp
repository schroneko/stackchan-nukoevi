#include "lvgl_port.h"
#include <M5Unified.h>
#include "M5GFX.h"

extern "C" {

#define LV_BUFFER_LINE 40
static SemaphoreHandle_t xGuiSemaphore;
static void lvgl_tick_timer(void *arg)
{
    (void)arg;
    lv_tick_inc(10);
}

static void lvgl_rtos_task(void *pvParameter)
{
    (void)pvParameter;
    while (1) {
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            lv_timer_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static lv_disp_draw_buf_t draw_buf;
static void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    M5GFX &gfx      = *(M5GFX *)disp->user_data;
    int w           = (area->x2 - area->x1 + 1);
    int h           = (area->y2 - area->y1 + 1);
    uint32_t pixels = w * h;

    M5.Display.startWrite();
    M5.Display.setAddrWindow(area->x1, area->y1, w, h);

    // Critical fix: Use safe pixel writing method to avoid M5GFX SIMD optimizations
    // Break large transfers into small chunks to avoid problematic copy_rgb_fast function
    const uint32_t SAFE_CHUNK_SIZE = 8192;  // 8K pixels per chunk, suitable for small buffer settings

    if (pixels > SAFE_CHUNK_SIZE) {
        // Chunked transmission for large data
        const lgfx::rgb565_t *src = (const lgfx::rgb565_t *)color_p;
        uint32_t remaining        = pixels;
        uint32_t offset           = 0;

        while (remaining > 0) {
            uint32_t chunk_size = (remaining > SAFE_CHUNK_SIZE) ? SAFE_CHUNK_SIZE : remaining;
            M5.Display.writePixels(src + offset, chunk_size);
            offset += chunk_size;
            remaining -= chunk_size;
        }
    } else {
        // Direct transmission for small data
        M5.Display.writePixels((lgfx::rgb565_t *)color_p, pixels);
    }

    M5.Display.endWrite();

    lv_disp_flush_ready(disp);
}

// BtnA / BtnB
static void lvgl_read_cb(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
}

void lvgl_port_init(void)
{
    lv_init();

    size_t buffer_size      = M5.Display.width() * LV_BUFFER_LINE * sizeof(lv_color_t);
    static lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    if (buf1 == NULL) {
        ESP_LOGE("LVGL", "Failed to allocate display buffer!");
        return;
    }

    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, M5.Display.width() * LV_BUFFER_LINE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res   = M5.Display.width();
    disp_drv.ver_res   = M5.Display.height();
    disp_drv.flush_cb  = lvgl_flush_cb;
    disp_drv.draw_buf  = &draw_buf;
    disp_drv.user_data = &M5.Display;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_BUTTON;
    indev_drv.read_cb = lvgl_read_cb;
    // indev_drv.user_data = &gfx;
    indev_drv.user_data = &M5.Display;
    lv_indev_t *indev   = lv_indev_drv_register(&indev_drv);

    xGuiSemaphore                                     = xSemaphoreCreateMutex();
    const esp_timer_create_args_t periodic_timer_args = {.callback = &lvgl_tick_timer, .name = "lvgl_tick_timer"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10 * 1000));
    xTaskCreate(lvgl_rtos_task, "lvgl_rtos_task", 4096, NULL, 1, NULL);
}

bool lvgl_port_lock(void)
{
    return xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE ? true : false;
}

void lvgl_port_unlock(void)
{
    xSemaphoreGive(xGuiSemaphore);
}
}