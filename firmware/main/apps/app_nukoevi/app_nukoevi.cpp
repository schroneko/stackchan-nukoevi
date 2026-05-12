#include "app_nukoevi.h"
#include <apps/common/common.h>
#include <assets/assets.h>
#include <hal/board/hal_bridge.h>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <stackchan/stackchan.h>
#include <ArduinoJson.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <smooth_lvgl.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/base64.h>

using namespace mooncake;
using namespace smooth_ui_toolkit::lvgl_cpp;

LV_IMAGE_DECLARE(nukoevi_icon);
LV_IMAGE_DECLARE(nukoevi_screen_open);
LV_IMAGE_DECLARE(nukoevi_screen_half_a);
LV_IMAGE_DECLARE(nukoevi_screen_closed);
LV_IMAGE_DECLARE(nukoevi_screen_half_b);
LV_FONT_DECLARE(font_puhui_14_1);

static std::unique_ptr<Container> _panel;
static std::unique_ptr<Image> _avatar;
static lv_obj_t* _caption_panel = nullptr;
static lv_obj_t* _llm_label     = nullptr;
static uint32_t _blink_timecount = 0;
static uint8_t _blink_index      = 0;
static bool _head_pet_receives                = false;
static int _head_pet_signal_connection        = -1;
static volatile bool _head_pet_requested      = false;
static bool _head_pet_active                  = false;
static uint8_t _head_pet_step                 = 0;
static uint32_t _head_pet_timecount           = 0;
static int _head_pet_base_yaw                 = 0;
static int _head_pet_base_pitch               = 0;
static bool _espnow_receives                  = false;
static bool _espnow_started                   = false;
static int _espnow_signal_connection          = -1;
static std::mutex _espnow_mutex;
static std::vector<uint8_t> _espnow_received_data;
static std::mutex _llm_mutex;
static bool _llm_running        = false;
static bool _llm_requested      = false;
static bool _llm_status_changed = false;
static std::string _llm_status;
static uint32_t _chat_request_id = 0;
static uint32_t _active_chat_request_id = 0;
static uint32_t _llm_started_at = 0;
static uint32_t _last_llm_request_at = 0;
static constexpr uint32_t _llm_timeout_ms = 60000;
static constexpr uint32_t _llm_request_interval_ms = 2500;
static uint32_t _caption_updated_at = 0;
static bool _caption_visible = false;
static bool _talk_requested = false;
static size_t _talk_request_text_size = 0;
static bool _talk_active = false;
static uint8_t _talk_index = 0;
static uint32_t _talk_timecount = 0;
static uint32_t _talk_until = 0;
static bool _sleep_mode = false;
static uint8_t _sleep_index = 0;
static uint32_t _sleep_timecount = 0;
static constexpr uint32_t _caption_auto_hide_ms = 6000;
static constexpr int _caption_width       = 316;
static constexpr int _caption_label_width = 300;
static constexpr uint8_t _nukoevi_backlight_brightness = 30;
static bool _last_touch_pressed = false;
static bool _motion_assets_loaded = false;
static lv_image_dsc_t _talk_closed;
static lv_image_dsc_t _talk_tiny;
static lv_image_dsc_t _talk_medium;
static lv_image_dsc_t _talk_wide;
static lv_image_dsc_t _talk_small;
static lv_image_dsc_t _talk_smile;
static lv_image_dsc_t _sleep_drowsy;
static lv_image_dsc_t _sleep_nearly_closed;
static lv_image_dsc_t _sleep_nod;
static lv_image_dsc_t _sleep_asleep;
static lv_image_dsc_t _sleep_wobble;
static lv_image_dsc_t _sleep_return;

static const lv_image_dsc_t* const _blink_sequence[] = {
    &nukoevi_screen_open,
    &nukoevi_screen_half_a,
    &nukoevi_screen_closed,
    &nukoevi_screen_half_b,
    &nukoevi_screen_open,
};

static const lv_image_dsc_t* const _talk_sequence[] = {
    &_talk_closed,
    &_talk_tiny,
    &_talk_medium,
    &_talk_wide,
    &_talk_small,
    &_talk_smile,
};

static const lv_image_dsc_t* const _sleep_sequence[] = {
    &_sleep_drowsy,
    &_sleep_nearly_closed,
    &_sleep_nod,
    &_sleep_asleep,
    &_sleep_wobble,
    &_sleep_return,
};

static const uint32_t _sleep_intervals[] = {
    3600,
    700,
    850,
    2400,
    850,
    900,
};

static void load_motion_assets()
{
    if (_motion_assets_loaded) {
        return;
    }

    _talk_closed = assets::get_image("nukoevi-talk-closed.bin");
    _talk_tiny = assets::get_image("nukoevi-talk-tiny.bin");
    _talk_medium = assets::get_image("nukoevi-talk-medium.bin");
    _talk_wide = assets::get_image("nukoevi-talk-wide.bin");
    _talk_small = assets::get_image("nukoevi-talk-small.bin");
    _talk_smile = assets::get_image("nukoevi-talk-smile.bin");
    _sleep_drowsy = assets::get_image("nukoevi-sleep-drowsy.bin");
    _sleep_nearly_closed = assets::get_image("nukoevi-sleep-nearly-closed.bin");
    _sleep_nod = assets::get_image("nukoevi-sleep-nod.bin");
    _sleep_asleep = assets::get_image("nukoevi-sleep-asleep.bin");
    _sleep_wobble = assets::get_image("nukoevi-sleep-wobble.bin");
    _sleep_return = assets::get_image("nukoevi-sleep-return.bin");
    _motion_assets_loaded = _talk_closed.data && _talk_tiny.data && _talk_medium.data && _talk_wide.data &&
                            _talk_small.data && _talk_smile.data && _sleep_drowsy.data &&
                            _sleep_nearly_closed.data && _sleep_nod.data && _sleep_asleep.data &&
                            _sleep_wobble.data && _sleep_return.data;
}

static bool starts_with(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

static bool should_talk_for_text(std::string_view text)
{
    if (text.empty()) {
        return false;
    }

    return !starts_with(text, "AI ") && !starts_with(text, "Open ") && !starts_with(text, "BLE ") &&
           !starts_with(text, "Starting ") && text != "考え中";
}

static bool is_night_time()
{
    const time_t now_t = time(nullptr);
    const time_t jst_t = now_t + 9 * 60 * 60;
    struct tm jst_tm;
    if (gmtime_r(&jst_t, &jst_tm) == nullptr) {
        return false;
    }

    if (jst_tm.tm_year < 124) {
        return false;
    }

    return jst_tm.tm_hour >= 22 || jst_tm.tm_hour < 7;
}

static void start_talk_animation(size_t text_size)
{
    const auto now = GetHAL().millis();
    const uint32_t duration = uitk::clamp<uint32_t>(1400 + static_cast<uint32_t>(text_size) * 40, 1800, 5200);

    _talk_active    = true;
    _talk_index     = 0;
    _talk_timecount = 0;
    _talk_until     = now + duration;
}

static void start_local_llm_request()
{
    const auto now = GetHAL().millis();
    std::lock_guard<std::mutex> lock(_llm_mutex);
    if (_llm_running || _llm_requested) {
        return;
    }
    if (_last_llm_request_at != 0 && now - _last_llm_request_at < _llm_request_interval_ms) {
        return;
    }

    _last_llm_request_at = now;
    _llm_requested = true;
}

static void update_llm_status(std::string_view status)
{
    std::lock_guard<std::mutex> lock(_llm_mutex);
    _llm_status         = status;
    _llm_status_changed = true;
}

static void finish_llm_request(uint32_t request_id, std::string_view status)
{
    std::lock_guard<std::mutex> lock(_llm_mutex);
    if (request_id != 0 && _active_chat_request_id != 0 && request_id != _active_chat_request_id) {
        return;
    }

    _llm_running            = false;
    _llm_requested          = false;
    _llm_started_at         = 0;
    _active_chat_request_id = 0;
    _llm_status             = status;
    _llm_status_changed     = true;
}

static bool notify_json(const ArduinoJson::JsonDocument& doc)
{
    std::string json;
    ArduinoJson::serializeJson(doc, json);
    return GetHAL().notifyBleConfig(json);
}

static std::string base64_encode(const uint8_t* data, size_t size)
{
    size_t encoded_size = 0;
    mbedtls_base64_encode(nullptr, 0, &encoded_size, data, size);
    std::string encoded(encoded_size, '\0');
    if (mbedtls_base64_encode(reinterpret_cast<unsigned char*>(encoded.data()), encoded.size(), &encoded_size, data,
                              size) != 0) {
        return {};
    }

    encoded.resize(encoded_size);
    return encoded;
}

static bool send_camera_capture(uint32_t request_id)
{
    auto camera = hal_bridge::board_get_camera();
    if (!camera) {
        return false;
    }

    if (!camera->StreamCaptures()) {
        return false;
    }

    const uint8_t* frame_data = camera->GetFrameData();
    const size_t frame_size = camera->GetFrameSize();
    const int width = camera->GetFrameWidth();
    const int height = camera->GetFrameHeight();
    const int format = camera->GetFrameFormat();
    uint8_t* jpeg_data = nullptr;
    size_t jpeg_size = 0;

    const bool encoded = image_to_jpeg(const_cast<uint8_t*>(frame_data), frame_size, width, height,
                                       static_cast<v4l2_pix_fmt_t>(format), 18, &jpeg_data, &jpeg_size);
    if (!encoded || !jpeg_data || jpeg_size == 0) {
        if (jpeg_data) {
            free(jpeg_data);
        }
        return false;
    }

    constexpr size_t chunk_size = 300;
    const size_t total_chunks = (jpeg_size + chunk_size - 1) / chunk_size;

    ArduinoJson::JsonDocument start_doc;
    start_doc["cmd"] = "cameraStart";
    start_doc["data"]["id"] = request_id;
    start_doc["data"]["size"] = jpeg_size;
    start_doc["data"]["chunks"] = total_chunks;
    start_doc["data"]["mime"] = "image/jpeg";
    if (!notify_json(start_doc)) {
        free(jpeg_data);
        return false;
    }
    GetHAL().delay(20);

    for (size_t index = 0; index < total_chunks; index++) {
        const size_t offset = index * chunk_size;
        const size_t part_size = std::min(chunk_size, jpeg_size - offset);
        const std::string encoded_chunk = base64_encode(jpeg_data + offset, part_size);
        if (encoded_chunk.empty()) {
            free(jpeg_data);
            return false;
        }

        ArduinoJson::JsonDocument chunk_doc;
        chunk_doc["cmd"] = "cameraChunk";
        chunk_doc["data"]["id"] = request_id;
        chunk_doc["data"]["index"] = index;
        chunk_doc["data"]["total"] = total_chunks;
        chunk_doc["data"]["data"] = encoded_chunk;
        if (!notify_json(chunk_doc)) {
            free(jpeg_data);
            return false;
        }
        GetHAL().delay(15);
    }

    free(jpeg_data);
    return true;
}

static bool send_chat_prompt(uint32_t request_id, bool has_image)
{
    ArduinoJson::JsonDocument doc;
    doc["cmd"] = "chatPrompt";
    doc["data"]["id"] = request_id;
    doc["data"]["hasImage"] = has_image;
    doc["data"]["text"] = has_image ? "ｽﾀｯｸﾁｬﾝのカメラ画像を見て、ぬこエビちゃんとして短く答えて。見えているものを一言で教えて。"
                                    : "ぬこエビちゃんとして、短く挨拶して。";
    return notify_json(doc);
}

static void local_llm_task(void* arg)
{
    const uint32_t request_id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg));

    update_llm_status("カメラ撮影中");
    const bool has_image = send_camera_capture(request_id);
    update_llm_status("考え中");

    if (!send_chat_prompt(request_id, has_image)) {
        finish_llm_request(request_id, "BLE send failed");
    }

    vTaskDelete(nullptr);
}

static void begin_local_llm_task()
{
    uint32_t request_id = 0;
    {
        std::lock_guard<std::mutex> lock(_llm_mutex);
        if (_llm_running) {
            return;
        }

        _llm_running            = true;
        _llm_requested          = false;
        _llm_status             = "カメラ撮影中";
        _llm_status_changed     = true;
        _llm_started_at         = GetHAL().millis();
        _active_chat_request_id = ++_chat_request_id;
        request_id              = _active_chat_request_id;
    }

    if (!GetHAL().isBleConnected()) {
        finish_llm_request(request_id, "Open iPhone app");
        return;
    }

    const auto arg = reinterpret_cast<void*>(static_cast<uintptr_t>(request_id));
    if (xTaskCreate(local_llm_task, "nukoevi_llm", 12288, arg, 3, nullptr) != pdPASS) {
        finish_llm_request(request_id, "Task start failed");
    }
}

static void handle_llm_timeout(uint32_t now)
{
    std::lock_guard<std::mutex> lock(_llm_mutex);
    if (!_llm_running || _llm_started_at == 0 || now - _llm_started_at < _llm_timeout_ms) {
        return;
    }

    _llm_running        = false;
    _llm_requested      = false;
    _llm_started_at     = 0;
    _active_chat_request_id = 0;
    _llm_status         = "Open iPhone app, then tap";
    _llm_status_changed = true;
    _talk_requested     = false;
    mclog::tagWarn("NUKOEVI", "LLM request timed out");
}

static std::string normalize_caption_text(const std::string& text)
{
    std::string normalized;
    normalized.reserve(text.size());

    bool pending_space = false;
    for (char c : text) {
        if (c == '\r' || c == '\n' || c == '\t' || c == ' ') {
            pending_space = true;
            continue;
        }

        if (pending_space && !normalized.empty()) {
            normalized.push_back(' ');
        }
        pending_space = false;
        normalized.push_back(c);
    }

    return normalized;
}

static bool remove_last_utf8_codepoint(std::string& text)
{
    if (text.empty()) {
        return false;
    }

    size_t offset = text.size() - 1;
    while (offset > 0 && (static_cast<uint8_t>(text[offset]) & 0xC0) == 0x80) {
        offset--;
    }

    text.erase(offset);
    return true;
}

static std::string fit_caption_text(const std::string& text, const lv_font_t* font)
{
    constexpr int max_lines = 3;
    const int max_height    = lv_font_get_line_height(font) * max_lines;

    lv_point_t text_size;
    lv_text_get_size(&text_size, text.c_str(), font, 0, 0, _caption_label_width, LV_TEXT_FLAG_NONE);
    if (text_size.y <= max_height) {
        return text;
    }

    std::string fitted = text;
    while (!fitted.empty()) {
        std::string candidate = fitted + "...";
        lv_text_get_size(&text_size, candidate.c_str(), font, 0, 0, _caption_label_width, LV_TEXT_FLAG_NONE);
        if (text_size.y <= max_height) {
            return candidate;
        }

        remove_last_utf8_codepoint(fitted);
    }

    return "...";
}

static void update_caption_text(const std::string& text)
{
    if (!_caption_panel || !_llm_label) {
        return;
    }

    const auto* font = &font_puhui_14_1;
    const std::string caption_text = fit_caption_text(normalize_caption_text(text), font);
    lv_point_t text_size;
    lv_text_get_size(&text_size, caption_text.c_str(), font, 0, 0, _caption_label_width, LV_TEXT_FLAG_NONE);

    const int line_height = lv_font_get_line_height(font);
    const bool single_line = text_size.y <= line_height + 2;
    const int label_height = single_line ? line_height : uitk::clamp(static_cast<int>(text_size.y), line_height * 2, line_height * 3);
    const int panel_height = label_height + 14;

    lv_obj_set_size(_caption_panel, _caption_width, panel_height);
    lv_obj_align(_caption_panel, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_size(_llm_label, _caption_label_width, label_height);
    lv_label_set_text(_llm_label, caption_text.c_str());
    lv_obj_align(_llm_label, LV_ALIGN_TOP_LEFT, 8, 6);
    lv_obj_clear_flag(_caption_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_caption_panel);
    _caption_visible    = true;
    _caption_updated_at = GetHAL().millis();
    mclog::tagInfo("NUKOEVI", "caption updated: {}", caption_text);
}

static void handle_caption_auto_hide(uint32_t now)
{
    if (!_caption_panel || !_caption_visible || _llm_running || now - _caption_updated_at < _caption_auto_hide_ms) {
        return;
    }

    lv_obj_add_flag(_caption_panel, LV_OBJ_FLAG_HIDDEN);
    _caption_visible = false;
    mclog::tagInfo("NUKOEVI", "caption hidden");
}

static bool update_talk_animation(uint32_t now)
{
    if (!_talk_active) {
        return false;
    }

    if (now >= _talk_until) {
        _talk_active    = false;
        _talk_index     = 0;
        _talk_timecount = 0;
        return false;
    }

    constexpr uint32_t talk_frame_ms = 120;
    if (_talk_timecount == 0 || now - _talk_timecount >= talk_frame_ms) {
        _talk_timecount = now;
        _talk_index = (_talk_index + 1) % (sizeof(_talk_sequence) / sizeof(_talk_sequence[0]));
        _avatar->setSrc(_talk_sequence[_talk_index]);
    }

    return true;
}

static bool update_sleep_animation(uint32_t now)
{
    if (!is_night_time()) {
        if (_sleep_mode) {
            _sleep_mode      = false;
            _sleep_index     = 0;
            _sleep_timecount = 0;
            _blink_index     = 0;
            _blink_timecount = now;
            _avatar->setSrc(&nukoevi_screen_open);
        }
        return false;
    }

    if (!_sleep_mode) {
        _sleep_mode      = true;
        _sleep_index     = 0;
        _sleep_timecount = now;
        _avatar->setSrc(_sleep_sequence[_sleep_index]);
        return true;
    }

    const uint32_t interval = _sleep_intervals[_sleep_index];
    if (now - _sleep_timecount >= interval) {
        _sleep_timecount = now;
        _sleep_index++;
        if (_sleep_index >= sizeof(_sleep_sequence) / sizeof(_sleep_sequence[0])) {
            _sleep_index = 0;
        }
        _avatar->setSrc(_sleep_sequence[_sleep_index]);
    }

    return true;
}

static void handle_screen_tap_request()
{
    const auto point = hal_bridge::get_touch_point();
    const bool pressed = point.num > 0;

    if (pressed && !_last_touch_pressed) {
        mclog::tagInfo("NUKOEVI", "raw touch press num={} x={} y={}", point.num, point.x, point.y);
        start_local_llm_request();
    } else if (!pressed && _last_touch_pressed) {
        mclog::tagInfo("NUKOEVI", "raw touch release");
    }

    _last_touch_pressed = pressed;
}

AppNukoevi::AppNukoevi()
{
    setAppInfo().name = "NUKOEVI";
    setAppInfo().icon = (void*)&nukoevi_icon;

    static uint32_t theme_color = 0xF5B06F;
    setAppInfo().userData       = (void*)&theme_color;
}

void AppNukoevi::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppNukoevi::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");
    GetHAL().startBleServer();
    GetHAL().setBackLightBrightness(_nukoevi_backlight_brightness, true);
    if (_espnow_signal_connection < 0) {
        _espnow_signal_connection = GetHAL().onEspNowData.connect([](const std::vector<uint8_t>& data) {
            if (!_espnow_receives) {
                return;
            }

            std::lock_guard<std::mutex> lock(_espnow_mutex);
            _espnow_received_data = data;
        });
    }
    _espnow_receives = true;
    if (!_espnow_started) {
        GetHAL().startEspNow(1);
        _espnow_started = true;
        mclog::tagInfo(getAppInfo().name, "ESP-NOW remote receiver ready on channel 1, id 1");
    }
    load_motion_assets();

    LvglLockGuard lock;

    _panel = std::make_unique<Container>(lv_screen_active());
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(320, 240);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setBgColor(lv_color_hex(0xF5B06F));
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _panel->setPadding(0, 0, 0, 0);

    _avatar = std::make_unique<Image>(_panel->get());
    _avatar->setSrc(&nukoevi_screen_open);
    _avatar->align(LV_ALIGN_CENTER, 0, 0);
    _blink_index     = 0;
    _blink_timecount = GetHAL().millis();
    _talk_active     = false;
    _talk_requested  = false;
    _sleep_mode      = false;
    _sleep_index     = 0;

    _caption_panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(_caption_panel, _caption_width, 44);
    lv_obj_align(_caption_panel, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_radius(_caption_panel, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(_caption_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(_caption_panel, lv_color_hex(0xFFF4E6), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_caption_panel, lv_color_hex(0x2B1710), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_caption_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_caption_panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_caption_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_caption_panel, LV_OBJ_FLAG_CLICKABLE);

    _llm_label = lv_label_create(_caption_panel);
    lv_obj_set_style_text_font(_llm_label, &font_puhui_14_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(_llm_label, lv_color_hex(0xFFF4E6), LV_PART_MAIN);
    lv_obj_set_style_text_align(_llm_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_long_mode(_llm_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(_llm_label, _caption_label_width, 34);
    lv_obj_align(_llm_label, LV_ALIGN_TOP_LEFT, 8, 6);
    lv_obj_clear_flag(_llm_label, LV_OBJ_FLAG_SCROLLABLE);
    update_caption_text("Open iPhone app, then tap");

    _panel->addFlag(LV_OBJ_FLAG_CLICKABLE);
    _panel->onClick().connect(start_local_llm_request);
    _avatar->addFlag(LV_OBJ_FLAG_CLICKABLE);
    _avatar->onClick().connect(start_local_llm_request);
    lv_obj_add_event_cb(_caption_panel, [](lv_event_t*) { start_local_llm_request(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(_llm_label, [](lv_event_t*) { start_local_llm_request(); }, LV_EVENT_CLICKED, nullptr);

    if (_head_pet_signal_connection < 0) {
        _head_pet_signal_connection = GetHAL().onHeadPetGesture.connect([](HeadPetGesture gesture) {
            if (!_head_pet_receives) {
                return;
            }

            if (gesture == HeadPetGesture::Press || gesture == HeadPetGesture::SwipeForward ||
                gesture == HeadPetGesture::SwipeBackward) {
                _head_pet_requested = true;
            }
        });
    }
    _head_pet_receives = true;

    GetHAL().onBleConfigData.connect([](const char* data) {
        ArduinoJson::JsonDocument doc;
        auto error = ArduinoJson::deserializeJson(doc, data);
        if (error) {
            return;
        }

        const char* cmd = doc["cmd"] | "";
        if (strcmp(cmd, "captureRequest") == 0) {
            start_local_llm_request();
            return;
        }
        if (strcmp(cmd, "chatResponse") != 0) {
            return;
        }

        const char* text = doc["data"]["text"] | "";
        const uint32_t response_id = doc["data"]["id"] | 0;
        std::lock_guard<std::mutex> lock(_llm_mutex);
        if (response_id != 0 && _active_chat_request_id != 0 && response_id != _active_chat_request_id) {
            mclog::tagWarn("NUKOEVI", "ignore stale LLM response id={}, active={}", response_id, _active_chat_request_id);
            return;
        }

        _llm_status         = text;
        _llm_status_changed = true;
        if (should_talk_for_text(text)) {
            _talk_requested = true;
            _talk_request_text_size = strlen(text);
        }
        _llm_running        = false;
        _llm_started_at     = 0;
        _active_chat_request_id = 0;
    });

    view::create_home_indicator([&]() { close(); }, 0xF8C489, 0x4A241A, lv_screen_active());
}

static void handle_head_pet_motion()
{
    auto& motion = GetStackChan().motion();
    const auto now = GetHAL().millis();

    if (_head_pet_requested) {
        _head_pet_requested = false;
        _head_pet_active    = true;
        _head_pet_step      = 0;
        _head_pet_timecount = 0;
        _head_pet_base_yaw   = motion.getCurrentYawAngle();
        _head_pet_base_pitch = motion.getCurrentPitchAngle();
    }

    if (!_head_pet_active) {
        return;
    }

    if (_head_pet_timecount != 0 && now - _head_pet_timecount < 150) {
        return;
    }

    _head_pet_timecount = now;

    const int happy_pitch = uitk::clamp(_head_pet_base_pitch + 80, 0, 540);
    const int speed       = 650;

    switch (_head_pet_step) {
        case 0:
            motion.moveWithSpeed(220, happy_pitch, speed);
            break;
        case 1:
            motion.moveWithSpeed(-220, happy_pitch, speed);
            break;
        case 2:
            motion.moveWithSpeed(160, happy_pitch, speed);
            break;
        case 3:
            motion.moveWithSpeed(-160, happy_pitch, speed);
            break;
        case 4:
            motion.moveWithSpeed(_head_pet_base_yaw, _head_pet_base_pitch, 420);
            break;
        default:
            _head_pet_active = false;
            return;
    }

    _head_pet_step++;
}

static void handle_espnow_remote_motion()
{
    std::vector<uint8_t> data;
    {
        std::lock_guard<std::mutex> lock(_espnow_mutex);
        data.swap(_espnow_received_data);
    }

    if (data.size() < 8) {
        return;
    }

    constexpr uint8_t receiver_id = 1;
    const uint8_t target_id = data[0];
    if (target_id != 0 && target_id != receiver_id) {
        return;
    }

    const int16_t yaw_angle = static_cast<int16_t>(data[1] | (data[2] << 8));
    const int16_t pitch_angle = static_cast<int16_t>(data[3] | (data[4] << 8));
    const int16_t speed = static_cast<int16_t>(data[5] | (data[6] << 8));

    mclog::tagInfo("NUKOEVI", "espnow remote yaw={} pitch={} speed={}", yaw_angle, pitch_angle, speed);
    _head_pet_active = false;
    GetStackChan().motion().moveWithSpeed(yaw_angle, pitch_angle, speed);
}

void AppNukoevi::onRunning()
{
    LvglLockGuard lock;
    view::update_home_indicator();
    handle_espnow_remote_motion();
    handle_head_pet_motion();
    GetStackChan().update();
    handle_screen_tap_request();

    constexpr uint32_t blink_interval_ms = 3200;
    constexpr uint32_t blink_frame_ms    = 85;
    const auto now                      = GetHAL().millis();
    handle_llm_timeout(now);
    handle_caption_auto_hide(now);

    if (!_avatar) {
        return;
    }

    bool should_start_llm = false;
    bool should_start_talk = false;
    size_t talk_text_size = 0;
    {
        std::lock_guard<std::mutex> lock(_llm_mutex);
        if (_llm_requested && !_llm_running) {
            _llm_requested = false;
            _llm_status = "Starting LLM...";
            _llm_status_changed = true;
            should_start_llm = true;
        }
        if (_llm_status_changed && _llm_label) {
            update_caption_text(_llm_status);
            _llm_status_changed = false;
        }
        if (_talk_requested) {
            _talk_requested = false;
            should_start_talk = true;
            talk_text_size = _talk_request_text_size;
            _talk_request_text_size = 0;
        }
    }

    if (should_start_llm) {
        begin_local_llm_task();
    }

    if (should_start_talk) {
        start_talk_animation(talk_text_size);
    }

    if (update_talk_animation(now)) {
        return;
    }

    if (update_sleep_animation(now)) {
        return;
    }

    if (_blink_index == 0) {
        if (now - _blink_timecount >= blink_interval_ms) {
            _blink_index     = 1;
            _blink_timecount = now;
            _avatar->setSrc(_blink_sequence[_blink_index]);
        }
        return;
    }

    if (now - _blink_timecount >= blink_frame_ms) {
        _blink_index++;
        if (_blink_index >= sizeof(_blink_sequence) / sizeof(_blink_sequence[0])) {
            _blink_index = 0;
        }
        _blink_timecount = now;
        _avatar->setSrc(_blink_sequence[_blink_index]);
    }
}

void AppNukoevi::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    LvglLockGuard lock;

    view::destroy_home_indicator();
    _head_pet_receives = false;
    _espnow_receives = false;
    {
        std::lock_guard<std::mutex> lock(_espnow_mutex);
        _espnow_received_data.clear();
    }
    GetHAL().onBleConfigData.clear();
    if (_caption_panel && lv_obj_is_valid(_caption_panel)) {
        lv_obj_delete(_caption_panel);
    }
    _llm_label     = nullptr;
    _caption_panel = nullptr;
    _caption_visible = false;
    _caption_updated_at = 0;
    _avatar.reset();
    _panel.reset();
}
