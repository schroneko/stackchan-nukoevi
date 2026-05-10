#include "app_nukoevi.h"
#include <apps/common/common.h>
#include <hal/board/hal_bridge.h>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <stackchan/stackchan.h>
#include <ArduinoJson.hpp>
#include <cstdint>
#include <memory>
#include <mutex>
#include <smooth_lvgl.hpp>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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
static std::mutex _llm_mutex;
static bool _llm_running        = false;
static bool _llm_requested      = false;
static bool _llm_status_changed = false;
static std::string _llm_status;
static uint32_t _chat_request_id = 0;
static uint32_t _active_chat_request_id = 0;
static uint32_t _llm_started_at = 0;
static uint32_t _last_llm_request_at = 0;
static constexpr uint32_t _llm_timeout_ms = 30000;
static constexpr uint32_t _llm_request_interval_ms = 2500;
static uint32_t _caption_updated_at = 0;
static bool _caption_visible = false;
static constexpr uint32_t _caption_auto_hide_ms = 6000;
static constexpr int _caption_width       = 316;
static constexpr int _caption_label_width = 300;
static constexpr uint8_t _nukoevi_backlight_brightness = 30;
static bool _last_touch_pressed = false;

static const lv_image_dsc_t* const _blink_sequence[] = {
    &nukoevi_screen_open,
    &nukoevi_screen_half_a,
    &nukoevi_screen_closed,
    &nukoevi_screen_half_b,
    &nukoevi_screen_open,
};

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

static void begin_local_llm_task()
{
    std::lock_guard<std::mutex> lock(_llm_mutex);
    if (!_llm_running) {
        _llm_running        = true;
        _llm_requested      = false;
        _llm_status         = "考え中";
        _llm_status_changed = true;
        _llm_started_at     = GetHAL().millis();
        _active_chat_request_id = ++_chat_request_id;

        ArduinoJson::JsonDocument doc;
        doc["cmd"]         = "chatPrompt";
        doc["data"]["id"]  = _active_chat_request_id;
        doc["data"]["text"] = "ぬこエビちゃんとして、短く挨拶して。";

        std::string json;
        ArduinoJson::serializeJson(doc, json);

        if (!GetHAL().isBleConnected()) {
            _llm_running        = false;
            _llm_status         = "Open iPhone app";
            _llm_status_changed = true;
            _llm_started_at     = 0;
            return;
        }

        if (!GetHAL().notifyBleConfig(json)) {
            _llm_running        = false;
            _llm_status         = "BLE send failed";
            _llm_status_changed = true;
            _llm_started_at     = 0;
        }
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
    GetHAL().setBackLightBrightness(_nukoevi_backlight_brightness, true);
    GetHAL().startBleServer();

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
        if (error || doc["cmd"] != "chatResponse") {
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

void AppNukoevi::onRunning()
{
    LvglLockGuard lock;
    view::update_home_indicator();
    handle_head_pet_motion();
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
    }

    if (should_start_llm) {
        begin_local_llm_task();
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
