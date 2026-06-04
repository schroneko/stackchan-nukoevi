#include "app_nukoevi.h"
#include <apps/common/common.h>
#include <apps/app_setup/app_setup.h>
#include <assets/assets.h>
#include <board.h>
#include <font_awesome.h>
#include <hal/board/hal_bridge.h>
#include <hal/hal.h>
#include <application.h>
#include <lvgl.h>
#include <mqtt.h>
#include <web_socket.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <stackchan/stackchan.h>
#include <ArduinoJson.hpp>
#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <queue>
#include <smooth_lvgl.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/base64.h>

using namespace mooncake;
using namespace smooth_ui_toolkit::lvgl_cpp;

LV_IMAGE_DECLARE(nukoevi_sleep_drowsy);
LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_20_4);
LV_FONT_DECLARE(font_awesome_30_4);

static std::unique_ptr<Container> _panel;
static std::unique_ptr<Image> _avatar;
static lv_obj_t* _mic_button = nullptr;
static lv_obj_t* _camera_button = nullptr;
static lv_obj_t* _camera_label = nullptr;
static lv_obj_t* _wifi_button = nullptr;
static lv_obj_t* _wifi_label = nullptr;
static lv_obj_t* _wifi_off_badge = nullptr;
static lv_obj_t* _home_button = nullptr;
static lv_obj_t* _home_label = nullptr;
static lv_obj_t* _battery_panel = nullptr;
static lv_obj_t* _battery_label = nullptr;
static lv_obj_t* _controls_scrim = nullptr;
static lv_obj_t* _controls_modal = nullptr;
static lv_obj_t* _brightness_label = nullptr;
static lv_obj_t* _brightness_slider = nullptr;
static lv_obj_t* _volume_label = nullptr;
static lv_obj_t* _volume_slider = nullptr;
static lv_obj_t* _external_led_label = nullptr;
static lv_obj_t* _external_led_slider = nullptr;
static lv_obj_t* _caption_panel = nullptr;
static lv_obj_t* _llm_label     = nullptr;
static lv_obj_t* _listen_indicator = nullptr;
static lv_obj_t* _listen_indicator_dot = nullptr;
enum class MicButtonState {
    Idle,
    Starting,
    Listening,
};
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
static int _xiaozhi_text_signal_connection    = -1;
static int _xiaozhi_status_signal_connection  = -1;
static std::mutex _espnow_mutex;
static std::vector<uint8_t> _espnow_received_data;
static std::mutex _llm_mutex;
static bool _llm_running        = false;
static bool _llm_requested      = false;
static bool _llm_status_changed = false;
static bool _caption_hide_requested = false;
static bool _caption_hide_after_audio = false;
static std::mutex _audio_playback_mutex;
static uint32_t _audio_playback_until = 0;
static uint32_t _caption_hold_until = 0;
static bool _caption_audio_waiting = false;
static bool _caption_audio_started = false;
static uint32_t _caption_audio_wait_until = 0;
static bool _listen_indicator_requested = false;
static std::string _llm_status;
static uint32_t _chat_request_id = 0;
static uint32_t _active_chat_request_id = 0;
static uint32_t _llm_started_at = 0;
static uint32_t _last_llm_request_at = 0;
static constexpr uint32_t _llm_timeout_ms = 60000;
static constexpr uint32_t _llm_request_interval_ms = 2500;
static uint32_t _caption_updated_at = 0;
static bool _caption_visible = false;
static bool _listen_indicator_visible = false;
static MicButtonState _mic_button_state_requested = MicButtonState::Idle;
static MicButtonState _mic_button_state_visible = MicButtonState::Idle;
static uint32_t _mic_button_event_at = 0;
static bool _mic_press_active = false;
static uint32_t _mic_pressed_at = 0;
static uint32_t _mic_touch_lost_at = 0;
static bool _xiaozhi_listening_started = false;
static bool _xiaozhi_text_waiting = false;
static uint32_t _xiaozhi_text_waiting_at = 0;
static bool _touch_point_active = false;
static uint32_t _touch_point_published_at = 0;
static int _last_touch_point_x = -1;
static int _last_touch_point_y = -1;
static bool _open_wifi_setup_requested = false;
static bool _open_home_requested = false;
static bool _network_started = false;
static bool _sleep_mode = false;
static uint8_t _sleep_index = 0;
static uint32_t _sleep_timecount = 0;
static constexpr uint32_t _caption_auto_hide_ms = 15000;
static constexpr uint32_t _caption_audio_start_wait_ms = 45000;
static constexpr uint32_t _caption_audio_end_grace_ms = 0;
static constexpr uint32_t _xiaozhi_text_timeout_ms = 4000;
static constexpr uint32_t _xiaozhi_start_timeout_ms = 10000;
static constexpr uint32_t _mic_min_hold_ms = 600;
static constexpr int _caption_width       = 316;
static constexpr int _caption_label_width = 300;
static constexpr int _top_mark_size = 54;
static constexpr int _top_mark_radius = _top_mark_size / 2;
static constexpr int _top_mark_y = 6;
static constexpr int _top_mark_x_wifi = 5;
static constexpr int _top_mark_x_home = 69;
static constexpr int _top_mark_x_battery = 133;
static constexpr int _top_mark_x_camera = 197;
static constexpr int _top_mark_x_mic = 261;
static constexpr uint8_t _nukoevi_backlight_brightness = 30;
static constexpr uint8_t _nukoevi_volume = 30;
static constexpr uint8_t _external_led_normal_brightness = 30;
static constexpr uint8_t _external_led_sleep_brightness = 10;
static constexpr std::array<uint8_t, 8> _brightness_levels = {1, 15, 30, 45, 60, 75, 90, 100};
static constexpr std::array<uint8_t, 11> _external_led_levels = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
static constexpr std::array<uint8_t, 21> _volume_levels = {
    0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100};
static constexpr bool _enable_espnow_remote = false;
static int32_t _brightness_index = 0;
static int32_t _volume_index = 0;
static int32_t _external_led_index = 0;
static bool _controls_syncing = false;
static bool _xiaozhi_interaction_requested = false;
static std::unique_ptr<Mqtt> _mqtt_output_client;
static std::mutex _mqtt_output_mutex;
static std::queue<std::string> _mqtt_output_messages;
static bool _mqtt_output_connecting = false;
static bool _mqtt_output_connected = false;
static uint32_t _mqtt_output_last_connect_at = 0;
static uint32_t _mqtt_output_disconnected_at = 0;
static std::mutex _mqtt_audio_queue_mutex;
static std::condition_variable _mqtt_audio_queue_cv;
static std::queue<std::unique_ptr<AudioStreamPacket>> _mqtt_audio_packets;
static TaskHandle_t _mqtt_audio_task_handle = nullptr;
static constexpr size_t _mqtt_audio_queue_limit = 160;
static std::string _mqtt_audio_id;
static int _mqtt_audio_expected_sequence = 0;
static int _mqtt_audio_total = 0;
static int _mqtt_audio_received = 0;
static int _mqtt_audio_gaps = 0;
static std::mutex _audio_ws_mutex;
static std::unique_ptr<WebSocket> _audio_ws_client;
static bool _audio_ws_connecting = false;
static bool _audio_ws_requested = false;
static bool _audio_ws_close_requested = false;
static uint32_t _audio_ws_last_connect_at = 0;
static std::string _audio_ws_audio_id;
static int _audio_ws_sample_rate = 16000;
static int _audio_ws_frame_duration = 60;
static int _audio_ws_total = 0;
static int _audio_ws_received = 0;
static constexpr const char* _mqtt_output_broker_host = "192.168.1.10";
static constexpr int _mqtt_output_broker_port = 18883;
static constexpr const char* _audio_ws_host = "192.168.1.10";
static constexpr int _audio_ws_port = 18080;
static constexpr const char* _mqtt_input_topic = "nukoevi/input/text";
static constexpr const char* _mqtt_output_topic = "nukoevi/output/text";
static constexpr const char* _mqtt_output_audio_topic = "nukoevi/output/audio/opus";
static constexpr const char* _mqtt_state_topic = "nukoevi/device/stackchan/state";
static constexpr const char* _blink_asset_names[] = {
    "nukoevi-screen-open.bin",
    "nukoevi-screen-half-a.bin",
    "nukoevi-screen-closed.bin",
    "nukoevi-screen-half-b.bin",
};
static constexpr size_t _blink_frame_count = sizeof(_blink_asset_names) / sizeof(_blink_asset_names[0]);
static constexpr uint8_t _blink_sequence_indices[] = {0, 1, 2, 3, 0};
static constexpr size_t _blink_sequence_count = sizeof(_blink_sequence_indices) / sizeof(_blink_sequence_indices[0]);
static std::array<lv_image_dsc_t, _blink_frame_count> _blink_assets;
static bool _blink_assets_loaded = false;

static constexpr const char* _sleep_asset_names[] = {
    "nukoevi-sleep-drowsy.bin",
    "nukoevi-sleep-nearly-closed.bin",
    "nukoevi-sleep-nod.bin",
    "nukoevi-sleep-asleep.bin",
    "nukoevi-sleep-wobble.bin",
    "nukoevi-sleep-return.bin",
};
static constexpr size_t _sleep_frame_count = sizeof(_sleep_asset_names) / sizeof(_sleep_asset_names[0]);
static std::array<lv_image_dsc_t, _sleep_frame_count> _sleep_assets;
static bool _sleep_assets_loaded = false;

static const uint32_t _sleep_intervals[] = {
    1200,
    1200,
    2400,
    2400,
    1200,
    1200,
};

static void begin_xiaozhi_voice_input();
static void end_xiaozhi_voice_input();
static void handle_mic_button_event(lv_event_t* event);
static void request_xiaozhi_stop_listening(const char* reason);
static void begin_evictl_camera_task();
static void publish_mqtt_input(const std::string& text, const char* role = nullptr);
static void publish_mqtt_state(const char* event_type, const std::string& text, const char* role = nullptr);

static bool is_valid_nukoevi_image(const lv_image_dsc_t* image)
{
    return image && image->data && image->data_size > 0 && image->header.magic == LV_IMAGE_HEADER_MAGIC &&
           image->header.w > 0 && image->header.h > 0;
}

static void load_blink_motion_assets()
{
    bool all_valid = true;

    for (size_t index = 0; index < _blink_frame_count; index++) {
        _blink_assets[index] = assets::get_image(_blink_asset_names[index]);
        if (!is_valid_nukoevi_image(&_blink_assets[index])) {
            all_valid = false;
            mclog::tagWarn("NUKOEVI", "blink asset {} invalid", _blink_asset_names[index]);
        }
    }

    _blink_assets_loaded = all_valid;
    mclog::tagInfo("NUKOEVI", "blink assets loaded: {}", all_valid ? "ok" : "fallback");
}

static const lv_image_dsc_t* get_blink_motion_frame(uint8_t index)
{
    if (_blink_assets_loaded && index < _blink_frame_count && is_valid_nukoevi_image(&_blink_assets[index])) {
        return &_blink_assets[index];
    }

    return &nukoevi_sleep_drowsy;
}

static void load_sleep_motion_assets()
{
    bool all_valid = true;

    for (size_t index = 0; index < _sleep_frame_count; index++) {
        _sleep_assets[index] = assets::get_image(_sleep_asset_names[index]);
        if (!is_valid_nukoevi_image(&_sleep_assets[index])) {
            all_valid = false;
            mclog::tagWarn("NUKOEVI", "sleep asset {} invalid", _sleep_asset_names[index]);
        }
    }

    _sleep_assets_loaded = all_valid;
    mclog::tagInfo("NUKOEVI", "sleep assets loaded: {}", all_valid ? "ok" : "fallback");
}

static const lv_image_dsc_t* get_sleep_motion_frame(uint8_t index)
{
    if (_sleep_assets_loaded && index < _sleep_frame_count && is_valid_nukoevi_image(&_sleep_assets[index])) {
        return &_sleep_assets[index];
    }

    return &nukoevi_sleep_drowsy;
}

static const lv_image_dsc_t* get_fallback_avatar_frame()
{
    return get_blink_motion_frame(0);
}

static bool set_avatar_motion_frame(const lv_image_dsc_t* image, const char* mode, uint8_t index)
{
    if (is_valid_nukoevi_image(image)) {
        _avatar->setSrc(image);
        return true;
    }

    mclog::tagWarn("NUKOEVI", "{} frame {} invalid, fallback to open image", mode, index);
    _avatar->setSrc(get_fallback_avatar_frame());
    return false;
}

static void enqueue_mqtt_output_payload(const std::string& payload)
{
    ArduinoJson::JsonDocument doc;
    auto error = ArduinoJson::deserializeJson(doc, payload);
    if (error) {
        return;
    }

    const char* target = doc["target"] | "";
    if (std::strlen(target) > 0 && std::strcmp(target, "stackchan") != 0 && std::strcmp(target, "all") != 0) {
        return;
    }

    const char* text = doc["text"] | "";
    if (std::strlen(text) == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(_mqtt_output_mutex);
    while (_mqtt_output_messages.size() >= 4) {
        _mqtt_output_messages.pop();
    }
    _mqtt_output_messages.emplace(text);

    _audio_ws_requested = true;
    _audio_ws_close_requested = false;
    GetHAL().startXiaozhiBackground();
}

static void mark_audio_playback_active(int sequence, int total, int frame_duration)
{
    const uint32_t now = GetHAL().millis();
    uint32_t active_ms = 2500;
    if (sequence >= 0 && total > 0 && sequence < total && frame_duration > 0) {
        const uint32_t remaining = static_cast<uint32_t>(total - sequence);
        const uint32_t frame_ms = std::max<uint32_t>(static_cast<uint32_t>(frame_duration), 100);
        active_ms = std::min<uint32_t>(remaining * frame_ms + 4000, 120000);
    }

    const uint32_t playback_until = now + active_ms;
    const uint32_t caption_until = playback_until + _caption_audio_end_grace_ms;
    std::lock_guard<std::mutex> lock(_audio_playback_mutex);
    _audio_playback_until = playback_until;
    _caption_audio_waiting = false;
    _caption_audio_started = true;
    _caption_audio_wait_until = 0;
    if (_caption_hold_until == 0 || static_cast<int32_t>(caption_until - _caption_hold_until) > 0) {
        _caption_hold_until = caption_until;
    }
}

static bool is_audio_playback_active(uint32_t now)
{
    std::lock_guard<std::mutex> lock(_audio_playback_mutex);
    return _audio_playback_until != 0 && static_cast<int32_t>(_audio_playback_until - now) > 0;
}

static void clear_pending_assistant_audio()
{
    {
        std::lock_guard<std::mutex> lock(_mqtt_audio_queue_mutex);
        while (!_mqtt_audio_packets.empty()) {
            _mqtt_audio_packets.pop();
        }
    }
    {
        std::lock_guard<std::mutex> lock(_audio_playback_mutex);
        _audio_playback_until = 0;
    }
}

static bool is_caption_hold_active(uint32_t now)
{
    std::lock_guard<std::mutex> lock(_audio_playback_mutex);
    return _caption_hold_until != 0 && static_cast<int32_t>(_caption_hold_until - now) > 0;
}

static void begin_caption_audio_wait(uint32_t now)
{
    std::lock_guard<std::mutex> lock(_audio_playback_mutex);
    _caption_audio_waiting = true;
    _caption_audio_started = false;
    _caption_audio_wait_until = now + _caption_audio_start_wait_ms;
    _caption_hold_until = 0;
}

static void reset_caption_audio_state()
{
    std::lock_guard<std::mutex> lock(_audio_playback_mutex);
    _caption_audio_waiting = false;
    _caption_audio_started = false;
    _caption_audio_wait_until = 0;
    _caption_hold_until = 0;
}

static bool is_caption_waiting_for_audio(uint32_t now)
{
    std::lock_guard<std::mutex> lock(_audio_playback_mutex);
    return _caption_audio_waiting && !_caption_audio_started && _caption_audio_wait_until != 0 &&
           static_cast<int32_t>(_caption_audio_wait_until - now) > 0;
}

static void track_mqtt_audio_packet(const char* audio_id, int sequence, int total, size_t decoded_size)
{
    const bool new_stream = sequence == 0 || _mqtt_audio_id != audio_id;
    if (new_stream) {
        _mqtt_audio_id = audio_id;
        _mqtt_audio_expected_sequence = 0;
        _mqtt_audio_total = total;
        _mqtt_audio_received = 0;
        _mqtt_audio_gaps = 0;

        char status[160];
        std::snprintf(status, sizeof(status), "id=%s total=%d bytes=%u", audio_id, total,
                      static_cast<unsigned>(decoded_size));
        publish_mqtt_state("audio.opus.start", status);
    }

    if (sequence >= 0) {
        if (sequence != _mqtt_audio_expected_sequence) {
            _mqtt_audio_gaps++;
            char status[160];
            std::snprintf(status, sizeof(status), "id=%s expected=%d got=%d total=%d", audio_id,
                          _mqtt_audio_expected_sequence, sequence, total);
            publish_mqtt_state("audio.opus.gap", status);
        }
        _mqtt_audio_expected_sequence = sequence + 1;
        _mqtt_audio_received++;
    }

    if (total > 0 && sequence == total - 1) {
        char status[160];
        std::snprintf(status, sizeof(status), "id=%s total=%d received=%d gaps=%d", audio_id, total,
                      _mqtt_audio_received, _mqtt_audio_gaps);
        publish_mqtt_state("audio.opus.done", status);
    }
}

static void publish_mqtt_audio_interrupted()
{
    if (_mqtt_audio_id.empty() || _mqtt_audio_total <= 0 || _mqtt_audio_received >= _mqtt_audio_total) {
        return;
    }

    char status[160];
    std::snprintf(status, sizeof(status), "id=%s total=%d received=%d expected=%d gaps=%d",
                  _mqtt_audio_id.c_str(), _mqtt_audio_total, _mqtt_audio_received, _mqtt_audio_expected_sequence,
                  _mqtt_audio_gaps);
    publish_mqtt_state("audio.opus.interrupted", status);
    _mqtt_audio_total = _mqtt_audio_received;
}

static void mqtt_audio_decode_task(void*)
{
    while (true) {
        std::unique_ptr<AudioStreamPacket> packet;
        {
            std::unique_lock<std::mutex> lock(_mqtt_audio_queue_mutex);
            _mqtt_audio_queue_cv.wait(lock, []() { return !_mqtt_audio_packets.empty(); });
            packet = std::move(_mqtt_audio_packets.front());
            _mqtt_audio_packets.pop();
        }

        if (packet) {
            Application::GetInstance().GetAudioService().PushPacketToDecodeQueue(std::move(packet), true);
        }
    }
}

static bool start_mqtt_audio_decode_task()
{
    if (_mqtt_audio_task_handle) {
        return true;
    }

    if (xTaskCreate(mqtt_audio_decode_task, "nukoevi_audio", 3072, nullptr, 4, &_mqtt_audio_task_handle) != pdPASS) {
        _mqtt_audio_task_handle = nullptr;
        mclog::tagWarn("NUKOEVI", "MQTT audio decode task start failed");
        return false;
    }
    return true;
}

static void enqueue_mqtt_audio_packet(std::unique_ptr<AudioStreamPacket> packet, const char* audio_id, int sequence,
                                      int total)
{
    if (!start_mqtt_audio_decode_task()) {
        publish_mqtt_state("audio.opus.decode_task_failed", "start failed");
        Application::GetInstance().GetAudioService().PushPacketToDecodeQueue(std::move(packet), true);
        return;
    }

    bool dropped = false;
    {
        std::lock_guard<std::mutex> lock(_mqtt_audio_queue_mutex);
        if (_mqtt_audio_packets.size() >= _mqtt_audio_queue_limit) {
            _mqtt_audio_packets.pop();
            dropped = true;
        }
        _mqtt_audio_packets.push(std::move(packet));
    }
    _mqtt_audio_queue_cv.notify_one();

    if (dropped) {
        char status[160];
        std::snprintf(status, sizeof(status), "id=%s seq=%d total=%d limit=%u", audio_id, sequence, total,
                      static_cast<unsigned>(_mqtt_audio_queue_limit));
        publish_mqtt_state("audio.opus.queue_drop", status);
    }
}

static void handle_mqtt_audio_payload(const std::string& payload)
{
    ArduinoJson::JsonDocument doc;
    auto error = ArduinoJson::deserializeJson(doc, payload);
    if (error) {
        return;
    }

    const char* target = doc["target"] | "";
    if (std::strlen(target) > 0 && std::strcmp(target, "stackchan") != 0 && std::strcmp(target, "all") != 0) {
        return;
    }

    const char* encoded = doc["payload"] | "";
    const auto encoded_size = std::strlen(encoded);
    if (encoded_size == 0) {
        return;
    }

    GetHAL().startXiaozhiBackground();

    size_t decoded_size = 0;
    auto ret = mbedtls_base64_decode(nullptr, 0, &decoded_size, reinterpret_cast<const unsigned char*>(encoded),
                                     encoded_size);
    if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL || decoded_size == 0) {
        return;
    }

    std::vector<uint8_t> decoded(decoded_size);
    ret = mbedtls_base64_decode(decoded.data(), decoded.size(), &decoded_size,
                                reinterpret_cast<const unsigned char*>(encoded), encoded_size);
    if (ret != 0 || decoded_size == 0) {
        return;
    }
    decoded.resize(decoded_size);

    const int sequence = doc["sequence"] | -1;
    const int total = doc["total"] | 0;
    const int frame_duration = doc["frame_duration"] | 60;
    const char* audio_id = doc["audio_id"] | "";
    mark_audio_playback_active(sequence, total, frame_duration);
    track_mqtt_audio_packet(audio_id, sequence, total, decoded.size());
    if (sequence == 0 || (total > 0 && sequence == total - 1)) {
        char status[64];
        std::snprintf(status, sizeof(status), "seq=%d total=%d bytes=%u", sequence, total,
                      static_cast<unsigned>(decoded.size()));
        publish_mqtt_state("audio.opus", status);
    }

    auto packet = std::make_unique<AudioStreamPacket>();
    packet->sample_rate = doc["sample_rate"] | 16000;
    packet->frame_duration = frame_duration;
    packet->timestamp = doc["timestamp"] | 0;
    packet->payload = std::move(decoded);
    enqueue_mqtt_audio_packet(std::move(packet), audio_id, sequence, total);
}

static void handle_audio_ws_text(const char* data, size_t len)
{
    ArduinoJson::JsonDocument doc;
    auto error = ArduinoJson::deserializeJson(doc, data, len);
    if (error) {
        return;
    }

    const char* type = doc["type"] | "";
    if (std::strcmp(type, "audio.start") == 0) {
        _audio_ws_audio_id = doc["audio_id"] | "";
        _audio_ws_sample_rate = doc["sample_rate"] | 16000;
        _audio_ws_frame_duration = doc["frame_duration"] | 60;
        _audio_ws_total = doc["total"] | 0;
        _audio_ws_received = 0;

        char status[160];
        std::snprintf(status, sizeof(status), "id=%s total=%d", _audio_ws_audio_id.c_str(), _audio_ws_total);
        publish_mqtt_state("audio.ws.start", status);
        return;
    }

    if (std::strcmp(type, "audio.stop") == 0) {
        char status[160];
        std::snprintf(status, sizeof(status), "id=%s total=%d received=%d", _audio_ws_audio_id.c_str(),
                      _audio_ws_total, _audio_ws_received);
        publish_mqtt_state("audio.ws.done", status);
        _audio_ws_requested = false;
        _audio_ws_close_requested = true;
        return;
    }
}

static void handle_audio_ws_binary(const char* data, size_t len)
{
    if (!data || len == 0) {
        return;
    }

    GetHAL().startXiaozhiBackground();

    const int sequence = _audio_ws_received;
    const int total = _audio_ws_total;
    const char* audio_id = _audio_ws_audio_id.empty() ? "ws" : _audio_ws_audio_id.c_str();
    mark_audio_playback_active(sequence, total, _audio_ws_frame_duration);
    track_mqtt_audio_packet(audio_id, sequence, total, len);

    auto packet = std::make_unique<AudioStreamPacket>();
    packet->sample_rate = _audio_ws_sample_rate;
    packet->frame_duration = _audio_ws_frame_duration;
    packet->timestamp = 0;
    packet->payload.assign(data, data + len);
    enqueue_mqtt_audio_packet(std::move(packet), audio_id, sequence, total);
    _audio_ws_received++;
}

static void publish_mqtt_state(const char* event_type, const std::string& text, const char* role)
{
    if (!_mqtt_output_client || !_mqtt_output_client->IsConnected()) {
        return;
    }

    ArduinoJson::JsonDocument doc;
    doc["type"] = event_type;
    doc["source"] = "stackchan";
    doc["target"] = "claude";
    doc["device_id"] = GetHAL().getFactoryMacString();
    doc["text"] = text;
    if (role && std::strlen(role) > 0) {
        doc["role"] = role;
    }

    std::string payload;
    ArduinoJson::serializeJson(doc, payload);
    _mqtt_output_client->Publish(_mqtt_state_topic, payload, 0);
}

static void publish_mqtt_input(const std::string& text, const char* role)
{
    if (!_mqtt_output_client || !_mqtt_output_client->IsConnected() || text.empty()) {
        return;
    }

    const auto device_id = GetHAL().getFactoryMacString();
    const auto now = GetHAL().millis();
    char request_id[96];
    std::snprintf(request_id, sizeof(request_id), "stackchan-%s-%lu", device_id.c_str(),
                  static_cast<unsigned long>(now));

    ArduinoJson::JsonDocument doc;
    doc["id"] = request_id;
    doc["type"] = "input.text";
    doc["source"] = "stackchan";
    doc["target"] = "claude";
    doc["session_id"] = "xiaozhi";
    doc["device_id"] = device_id;
    doc["text"] = text;
    if (role && std::strlen(role) > 0) {
        doc["role"] = role;
    }

    std::string payload;
    ArduinoJson::serializeJson(doc, payload);
    _mqtt_output_client->Publish(_mqtt_input_topic, payload, 1);
    publish_mqtt_state("mqtt.input.published", text, role);
}

static void close_audio_ws_receiver()
{
    std::lock_guard<std::mutex> lock(_audio_ws_mutex);
    if (_audio_ws_client) {
        _audio_ws_client->Close();
        _audio_ws_client.reset();
    }
    _audio_ws_close_requested = false;
}

static void audio_ws_connect_task(void*)
{
    auto network = Board::GetInstance().GetNetwork();
    if (!network) {
        _audio_ws_connecting = false;
        vTaskDelete(nullptr);
        return;
    }

    auto ws = network->CreateWebSocket(3);
    if (!ws) {
        mclog::tagWarn("NUKOEVI", "audio websocket create failed");
        _audio_ws_connecting = false;
        vTaskDelete(nullptr);
        return;
    }

    const auto device_id = GetHAL().getFactoryMacString();
    ws->SetHeader("Device-Id", device_id.c_str());
    ws->SetHeader("Client-Id", device_id.c_str());
    ws->SetReceiveBufferSize(4096);
    ws->OnConnected([]() { publish_mqtt_state("audio.ws.connected", "connected"); });
    ws->OnDisconnected([]() { publish_mqtt_state("audio.ws.disconnected", "disconnected"); });
    ws->OnError([](int error) {
        char status[64];
        std::snprintf(status, sizeof(status), "error=%d", error);
        publish_mqtt_state("audio.ws.error", status);
    });
    ws->OnData([](const char* data, size_t len, bool binary) {
        if (binary) {
            handle_audio_ws_binary(data, len);
            return;
        }
        handle_audio_ws_text(data, len);
    });

    if (!_audio_ws_requested) {
        _audio_ws_connecting = false;
        vTaskDelete(nullptr);
        return;
    }

    const std::string url = fmt::format("ws://{}:{}/audio", _audio_ws_host, _audio_ws_port);
    if (ws->Connect(url.c_str())) {
        std::lock_guard<std::mutex> lock(_audio_ws_mutex);
        if (_audio_ws_requested) {
            _audio_ws_client = std::move(ws);
        } else {
            ws->Close();
        }
    } else {
        mclog::tagWarn("NUKOEVI", "audio websocket connect failed");
    }

    _audio_ws_connecting = false;
    vTaskDelete(nullptr);
}

static void ensure_audio_ws_receiver()
{
    if (_audio_ws_close_requested) {
        close_audio_ws_receiver();
    }
    if (!_audio_ws_requested || _mic_press_active || _xiaozhi_interaction_requested || GetHAL().isXiaozhiListening()) {
        return;
    }
    if (GetHAL().getWifiStatus() == WifiStatus::None) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(_audio_ws_mutex);
        if (_audio_ws_client && _audio_ws_client->IsConnected()) {
            return;
        }
    }
    if (_audio_ws_connecting) {
        return;
    }

    const auto now = GetHAL().millis();
    if (_audio_ws_last_connect_at != 0 && now - _audio_ws_last_connect_at < 5000) {
        return;
    }
    _audio_ws_last_connect_at = now;
    _audio_ws_connecting = true;

    if (xTaskCreate(audio_ws_connect_task, "nukoevi_audio_ws", 4096, nullptr, 3, nullptr) != pdPASS) {
        _audio_ws_connecting = false;
        mclog::tagWarn("NUKOEVI", "audio websocket task failed");
    }
}

static void mqtt_output_connect_task(void*)
{
    bool connected = false;
    if (_mqtt_output_client) {
        const auto client_id = "nukoevi-stackchan-" + GetHAL().getFactoryMacString();
        connected = _mqtt_output_client->Connect(_mqtt_output_broker_host, _mqtt_output_broker_port, client_id, "", "");
    }
    if (!connected) {
        _mqtt_output_connected = false;
        _mqtt_output_disconnected_at = GetHAL().millis();
        mclog::tagWarn("NUKOEVI", "MQTT output receiver connect failed");
    }
    _mqtt_output_connecting = false;
    vTaskDelete(nullptr);
}

static bool is_mqtt_output_connected()
{
    return _mqtt_output_client && _mqtt_output_client->IsConnected();
}

static void ensure_mqtt_output_receiver()
{
    if (GetHAL().getWifiStatus() == WifiStatus::None) {
        _mqtt_output_connected = false;
        _mqtt_output_disconnected_at = GetHAL().millis();
        return;
    }
    if (_mqtt_output_client && _mqtt_output_client->IsConnected()) {
        _mqtt_output_connected = true;
        _mqtt_output_disconnected_at = 0;
        return;
    }
    if (_mqtt_output_connecting) {
        return;
    }

    const auto now = GetHAL().millis();
    if (_mqtt_output_last_connect_at != 0 && now - _mqtt_output_last_connect_at < 5000) {
        return;
    }
    _mqtt_output_last_connect_at = now;

    auto network = Board::GetInstance().GetNetwork();
    if (!network) {
        return;
    }

    _mqtt_output_client = network->CreateMqtt(2);
    if (!_mqtt_output_client) {
        mclog::tagWarn("NUKOEVI", "MQTT output receiver create failed");
        return;
    }

    _mqtt_output_client->OnConnected([]() {
        mclog::tagInfo("NUKOEVI", "MQTT output receiver connected");
        _mqtt_output_connected = true;
        _mqtt_output_disconnected_at = 0;
        _mqtt_output_client->Subscribe(_mqtt_output_topic, 0);
        _mqtt_output_client->Subscribe(_mqtt_output_audio_topic, 1);
        publish_mqtt_audio_interrupted();
        publish_mqtt_state("mqtt.connected", "connected");
    });
    _mqtt_output_client->OnDisconnected([]() {
        _mqtt_output_connected = false;
        _mqtt_output_disconnected_at = GetHAL().millis();
        mclog::tagInfo("NUKOEVI", "MQTT output receiver disconnected");
    });
    _mqtt_output_client->OnMessage([](const std::string& topic, const std::string& payload) {
        if (topic == _mqtt_output_audio_topic) {
            handle_mqtt_audio_payload(payload);
            return;
        }
        if (topic == _mqtt_output_topic) {
            enqueue_mqtt_output_payload(payload);
        }
    });

    _mqtt_output_connecting = true;
    if (xTaskCreate(mqtt_output_connect_task, "nukoevi_mqtt", 4096, nullptr, 3, nullptr) != pdPASS) {
        _mqtt_output_connecting = false;
        mclog::tagWarn("NUKOEVI", "MQTT output receiver task failed");
    }
}

static void handle_mqtt_output_connection_watchdog(uint32_t now)
{
    if (GetHAL().getWifiStatus() == WifiStatus::None) {
        _mqtt_output_connected = false;
        _mqtt_output_disconnected_at = now;
        return;
    }
    if (is_mqtt_output_connected()) {
        _mqtt_output_connected = true;
        _mqtt_output_disconnected_at = 0;
        return;
    }
    _mqtt_output_connected = false;
    if (_mqtt_output_disconnected_at == 0) {
        _mqtt_output_disconnected_at = now;
    }
    if (!_mqtt_output_connecting && now - _mqtt_output_disconnected_at >= 15000) {
        _mqtt_output_client.reset();
        _mqtt_output_last_connect_at = 0;
        _mqtt_output_disconnected_at = now;
        mclog::tagWarn("NUKOEVI", "MQTT output receiver stale, recreating client");
    }
    ensure_mqtt_output_receiver();
}

static int32_t value_to_index(uint8_t value, const uint8_t* levels, size_t size)
{
    if (size == 0) {
        return 0;
    }

    size_t best_index = 0;
    uint8_t best_distance = levels[0] > value ? levels[0] - value : value - levels[0];
    for (size_t i = 0; i < size; i++) {
        const uint8_t distance = levels[i] > value ? levels[i] - value : value - levels[i];
        if (distance < best_distance) {
            best_distance = distance;
            best_index = i;
        }
    }

    return static_cast<int32_t>(best_index);
}

static void snap_slider_to_level(lv_obj_t* slider, uint8_t value)
{
    _controls_syncing = true;
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
    _controls_syncing = false;
}

static void set_button_style(lv_obj_t* button, uint32_t bg_color, uint32_t text_color)
{
    lv_obj_set_size(button, _top_mark_size, _top_mark_size);
    lv_obj_set_style_radius(button, _top_mark_radius, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(0xFFF4E6), LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(bg_color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_set_style_text_color(button, lv_color_hex(text_color), LV_PART_MAIN);
    lv_obj_set_style_text_font(button, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
}

static void update_controls_labels()
{
    char text[24];
    if (_brightness_label) {
        std::snprintf(text, sizeof(text), "Display %u%%", _brightness_levels[_brightness_index]);
        lv_label_set_text(_brightness_label, text);
    }
    if (_volume_label) {
        std::snprintf(text, sizeof(text), "Volume %u%%", _volume_levels[_volume_index]);
        lv_label_set_text(_volume_label, text);
    }
    if (_external_led_label) {
        std::snprintf(text, sizeof(text), "LED %u%%", _external_led_levels[_external_led_index]);
        lv_label_set_text(_external_led_label, text);
    }
}

static void hide_controls_modal()
{
    if (_controls_scrim) {
        lv_obj_add_flag(_controls_scrim, LV_OBJ_FLAG_HIDDEN);
    }
}

static void open_setup_wifi()
{
    mclog::tagInfo("NUKOEVI", "open Wi-Fi setup requested");
    _open_wifi_setup_requested = true;
}

static void open_home()
{
    mclog::tagInfo("NUKOEVI", "open home requested");
    _open_home_requested = true;
}

static void start_network_once()
{
    if (_network_started) {
        return;
    }

    _network_started = true;
    mclog::tagInfo("NUKOEVI", "start Wi-Fi network");
    Board::GetInstance().StartNetwork();
}

static bool open_setup_wifi_if_requested()
{
    if (!_open_wifi_setup_requested) {
        return false;
    }

    _open_wifi_setup_requested = false;
    AppSetup::requestOpenWifiSetup();
    const auto app_num = GetMooncake().getAppNum();
    for (std::size_t i = 0; i < app_num; i++) {
        const auto info = GetMooncake().getAppInfo(static_cast<int>(i));
        if (info.name == "SETUP") {
            GetMooncake().openApp(static_cast<int>(i));
            return true;
        }
    }
    mclog::tagWarn("NUKOEVI", "SETUP app not found");
    return false;
}

static bool open_home_if_requested()
{
    if (!_open_home_requested) {
        return false;
    }

    _open_home_requested = false;
    const auto app_num = GetMooncake().getAppNum();
    for (std::size_t i = 0; i < app_num; i++) {
        const auto info = GetMooncake().getAppInfo(static_cast<int>(i));
        if (info.name == "Launcher") {
            GetMooncake().openApp(static_cast<int>(i));
            return true;
        }
    }
    mclog::tagWarn("NUKOEVI", "Launcher app not found");
    return false;
}

static void update_wifi_button()
{
    if (!_wifi_button || !_wifi_label || !_wifi_off_badge) {
        return;
    }

    const bool connected = GetHAL().getWifiStatus() != WifiStatus::None;
    lv_label_set_text(_wifi_label, LV_SYMBOL_WIFI);
    lv_obj_set_style_bg_color(_wifi_button, lv_color_hex(connected ? 0x2B1710 : 0x4A3B3B), LV_PART_MAIN);
    lv_obj_set_style_text_color(_wifi_label, lv_color_hex(connected ? 0xFFF4E6 : 0xC8B8B8), LV_PART_MAIN);

    if (connected) {
        lv_obj_add_flag(_wifi_off_badge, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(_wifi_off_badge, LV_OBJ_FLAG_HIDDEN);
    }
}

static void update_mic_button(MicButtonState state)
{
    if (!_mic_button) {
        return;
    }
    if (_mic_button_state_visible == state) {
        return;
    }

    uint32_t bg_color = 0x2B1710;
    uint32_t mark_color = 0xFFF4E6;
    if (state == MicButtonState::Starting) {
        bg_color = 0x5A3518;
        mark_color = 0xF5B06F;
    } else if (state == MicButtonState::Listening) {
        bg_color = 0x123F35;
        mark_color = 0x9CFFB5;
    }

    lv_obj_set_style_bg_color(_mic_button, lv_color_hex(bg_color), LV_PART_MAIN);
    lv_obj_set_style_border_color(_mic_button, lv_color_hex(mark_color), LV_PART_MAIN);
    for (uint32_t i = 0; i < lv_obj_get_child_count(_mic_button); i++) {
        auto child = lv_obj_get_child(_mic_button, i);
        lv_obj_set_style_bg_color(child, lv_color_hex(mark_color), LV_PART_MAIN);
    }
    _mic_button_state_visible = state;
}

static const char* get_battery_symbol(uint8_t level)
{
    if (level >= 90) {
        return LV_SYMBOL_BATTERY_FULL;
    }
    if (level >= 65) {
        return LV_SYMBOL_BATTERY_3;
    }
    if (level >= 40) {
        return LV_SYMBOL_BATTERY_2;
    }
    if (level >= 15) {
        return LV_SYMBOL_BATTERY_1;
    }
    return LV_SYMBOL_BATTERY_EMPTY;
}

static void update_battery_indicator()
{
    if (!_battery_panel || !_battery_label) {
        return;
    }

    const auto level = GetHAL().getBatteryLevel();
    const bool charging = GetHAL().isBatteryCharging();
    lv_label_set_text(_battery_label, get_battery_symbol(level));

    const uint32_t bg_color = level <= 15 ? 0x5A2525 : 0x2B1710;
    const uint32_t text_color = charging ? 0x9CFFB5 : (level <= 15 ? 0xFFB3A7 : 0xFFF4E6);
    const uint32_t border_color = charging ? 0x9CFFB5 : 0xFFF4E6;
    lv_obj_set_style_bg_color(_battery_panel, lv_color_hex(bg_color), LV_PART_MAIN);
    lv_obj_set_style_border_color(_battery_panel, lv_color_hex(border_color), LV_PART_MAIN);
    lv_obj_set_style_text_color(_battery_label, lv_color_hex(text_color), LV_PART_MAIN);
}

static void create_battery_indicator()
{
    _battery_panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(_battery_panel, _top_mark_size, _top_mark_size);
    lv_obj_align(_battery_panel, LV_ALIGN_TOP_LEFT, _top_mark_x_battery, _top_mark_y);
    lv_obj_set_style_radius(_battery_panel, _top_mark_radius, LV_PART_MAIN);
    lv_obj_set_style_border_width(_battery_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(_battery_panel, lv_color_hex(0xFFF4E6), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_battery_panel, lv_color_hex(0x2B1710), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_battery_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(_battery_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_battery_panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_battery_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(_battery_panel, LV_OBJ_FLAG_CLICKABLE);

    _battery_label = lv_label_create(_battery_panel);
    lv_obj_set_style_text_font(_battery_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(_battery_label, lv_color_hex(0xFFF4E6), LV_PART_MAIN);
    lv_obj_center(_battery_label);
    update_battery_indicator();
}

static void create_control_button(lv_obj_t** button, lv_obj_t** label, lv_align_t align, int x, int y, const char* text,
                                  lv_event_cb_t callback)
{
    *button = lv_obj_create(lv_screen_active());
    set_button_style(*button, 0x2B1710, 0xFFF4E6);
    lv_obj_align(*button, align, x, y);
    if (callback) {
        lv_obj_add_event_cb(*button, callback, LV_EVENT_CLICKED, nullptr);
    }

    *label = lv_label_create(*button);
    lv_label_set_text(*label, text);
    lv_obj_center(*label);
}

static void create_mic_icon(lv_obj_t* parent)
{
    auto create_part = [&](int width, int height, int radius, lv_align_t align, int x, int y) {
        auto part = lv_obj_create(parent);
        lv_obj_set_size(part, width, height);
        lv_obj_align(part, align, x, y);
        lv_obj_set_style_radius(part, radius, LV_PART_MAIN);
        lv_obj_set_style_border_width(part, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(part, lv_color_hex(0xFFF4E6), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(part, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_pad_all(part, 0, LV_PART_MAIN);
        lv_obj_clear_flag(part, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(part, LV_OBJ_FLAG_CLICKABLE);
    };

    create_part(18, 27, 9, LV_ALIGN_TOP_MID, 0, 8);
    create_part(5, 10, 2, LV_ALIGN_BOTTOM_MID, 0, -10);
    create_part(24, 5, 2, LV_ALIGN_BOTTOM_MID, 0, -7);
}

static void create_top_controls()
{
    lv_obj_t* mic_label = nullptr;
    create_control_button(&_mic_button, &mic_label, LV_ALIGN_TOP_LEFT, _top_mark_x_mic, _top_mark_y, "", nullptr);
    lv_obj_add_event_cb(_mic_button, handle_mic_button_event, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(_mic_button, handle_mic_button_event, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(_mic_button, handle_mic_button_event, LV_EVENT_PRESS_LOST, nullptr);
    create_mic_icon(_mic_button);

    create_control_button(&_camera_button, &_camera_label, LV_ALIGN_TOP_LEFT, _top_mark_x_camera, _top_mark_y,
                          FONT_AWESOME_CAMERA,
                          [](lv_event_t*) { begin_evictl_camera_task(); });
    lv_obj_set_style_text_font(_camera_label, &font_awesome_30_4, LV_PART_MAIN);

    create_control_button(&_wifi_button, &_wifi_label, LV_ALIGN_TOP_LEFT, _top_mark_x_wifi, _top_mark_y, LV_SYMBOL_WIFI,
                          [](lv_event_t*) { open_setup_wifi(); });

    create_control_button(&_home_button, &_home_label, LV_ALIGN_TOP_LEFT, _top_mark_x_home, _top_mark_y,
                          FONT_AWESOME_HOUSE,
                          [](lv_event_t*) { open_home(); });
    lv_obj_set_style_text_font(_home_label, &font_awesome_30_4, LV_PART_MAIN);

    _wifi_off_badge = lv_label_create(_wifi_button);
    lv_label_set_text(_wifi_off_badge, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(_wifi_off_badge, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(_wifi_off_badge, lv_color_hex(0xFFB3A7), LV_PART_MAIN);
    lv_obj_align(_wifi_off_badge, LV_ALIGN_BOTTOM_RIGHT, -2, -1);
    update_wifi_button();

    create_battery_indicator();
}

static void create_controls_modal()
{
    _controls_scrim = lv_obj_create(lv_screen_active());
    lv_obj_set_size(_controls_scrim, 320, 240);
    lv_obj_align(_controls_scrim, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(_controls_scrim, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_controls_scrim, 110, LV_PART_MAIN);
    lv_obj_set_style_border_width(_controls_scrim, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_controls_scrim, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_controls_scrim, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_controls_scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_controls_scrim, [](lv_event_t*) { hide_controls_modal(); }, LV_EVENT_CLICKED, nullptr);

    _controls_modal = lv_obj_create(_controls_scrim);
    lv_obj_set_size(_controls_modal, 300, 216);
    lv_obj_align(_controls_modal, LV_ALIGN_CENTER, 0, 4);
    lv_obj_set_style_radius(_controls_modal, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(_controls_modal, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(_controls_modal, lv_color_hex(0xFFF4E6), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_controls_modal, lv_color_hex(0x2B1710), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_controls_modal, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_controls_modal, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_controls_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_controls_modal, LV_OBJ_FLAG_CLICKABLE);

    auto close_button = lv_obj_create(_controls_modal);
    set_button_style(close_button, 0xFFF4E6, 0x2B1710);
    lv_obj_set_size(close_button, 30, 30);
    lv_obj_align(close_button, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_add_event_cb(close_button, [](lv_event_t*) { hide_controls_modal(); }, LV_EVENT_CLICKED, nullptr);
    auto close_label = lv_label_create(close_button);
    lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
    lv_obj_center(close_label);

    _brightness_label = lv_label_create(_controls_modal);
    lv_obj_set_style_text_font(_brightness_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(_brightness_label, lv_color_hex(0xFFF4E6), LV_PART_MAIN);
    lv_obj_align(_brightness_label, LV_ALIGN_TOP_LEFT, 18, 22);

    _brightness_slider = lv_slider_create(_controls_modal);
    lv_obj_set_size(_brightness_slider, 250, 12);
    lv_obj_align(_brightness_slider, LV_ALIGN_TOP_MID, 0, 50);
    lv_slider_set_range(_brightness_slider, 0, 100);
    lv_obj_set_style_bg_color(_brightness_slider, lv_color_hex(0xFFF4E6), LV_PART_KNOB);
    lv_obj_set_style_bg_color(_brightness_slider, lv_color_hex(0xF5B06F), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_brightness_slider, lv_color_hex(0x6D597A), LV_PART_MAIN);
    lv_obj_add_event_cb(
        _brightness_slider,
        [](lv_event_t* event) {
            if (_controls_syncing) {
                return;
            }
            auto slider = static_cast<lv_obj_t*>(lv_event_get_target(event));
            _brightness_index = value_to_index(lv_slider_get_value(slider), _brightness_levels.data(),
                                               _brightness_levels.size());
            snap_slider_to_level(slider, _brightness_levels[_brightness_index]);
            update_controls_labels();
            GetHAL().setBackLightBrightness(_brightness_levels[_brightness_index], false);
        },
        LV_EVENT_VALUE_CHANGED, nullptr);

    _volume_label = lv_label_create(_controls_modal);
    lv_obj_set_style_text_font(_volume_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(_volume_label, lv_color_hex(0xFFF4E6), LV_PART_MAIN);
    lv_obj_align(_volume_label, LV_ALIGN_TOP_LEFT, 18, 86);

    _volume_slider = lv_slider_create(_controls_modal);
    lv_obj_set_size(_volume_slider, 250, 12);
    lv_obj_align(_volume_slider, LV_ALIGN_TOP_MID, 0, 114);
    lv_slider_set_range(_volume_slider, 0, 100);
    lv_obj_set_style_bg_color(_volume_slider, lv_color_hex(0xFFF4E6), LV_PART_KNOB);
    lv_obj_set_style_bg_color(_volume_slider, lv_color_hex(0xF5B06F), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_volume_slider, lv_color_hex(0x6D597A), LV_PART_MAIN);
    lv_obj_add_event_cb(
        _volume_slider,
        [](lv_event_t* event) {
            if (_controls_syncing) {
                return;
            }
            auto slider = static_cast<lv_obj_t*>(lv_event_get_target(event));
            _volume_index = value_to_index(lv_slider_get_value(slider), _volume_levels.data(), _volume_levels.size());
            snap_slider_to_level(slider, _volume_levels[_volume_index]);
            update_controls_labels();
            GetHAL().setSpeakerVolume(_volume_levels[_volume_index], false);
        },
        LV_EVENT_VALUE_CHANGED, nullptr);

    _external_led_label = lv_label_create(_controls_modal);
    lv_obj_set_style_text_font(_external_led_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(_external_led_label, lv_color_hex(0xFFF4E6), LV_PART_MAIN);
    lv_obj_align(_external_led_label, LV_ALIGN_TOP_LEFT, 18, 150);

    _external_led_slider = lv_slider_create(_controls_modal);
    lv_obj_set_size(_external_led_slider, 250, 12);
    lv_obj_align(_external_led_slider, LV_ALIGN_TOP_MID, 0, 178);
    lv_slider_set_range(_external_led_slider, 0, 100);
    lv_obj_set_style_bg_color(_external_led_slider, lv_color_hex(0xFFF4E6), LV_PART_KNOB);
    lv_obj_set_style_bg_color(_external_led_slider, lv_color_hex(0xF5B06F), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_external_led_slider, lv_color_hex(0x6D597A), LV_PART_MAIN);
    lv_obj_add_event_cb(
        _external_led_slider,
        [](lv_event_t* event) {
            if (_controls_syncing) {
                return;
            }
            auto slider = static_cast<lv_obj_t*>(lv_event_get_target(event));
            _external_led_index = value_to_index(lv_slider_get_value(slider), _external_led_levels.data(),
                                                 _external_led_levels.size());
            snap_slider_to_level(slider, _external_led_levels[_external_led_index]);
            update_controls_labels();
            GetHAL().setExternalLedBrightness(_external_led_levels[_external_led_index],
                                              _external_led_levels[_external_led_index], false);
        },
        LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_add_flag(_controls_scrim, LV_OBJ_FLAG_HIDDEN);
}

static void show_controls_modal()
{
    if (!_controls_scrim) {
        create_controls_modal();
    }

    _brightness_index = value_to_index(GetHAL().getBackLightBrightness(), _brightness_levels.data(), _brightness_levels.size());
    _volume_index = value_to_index(GetHAL().getSpeakerVolume(), _volume_levels.data(), _volume_levels.size());
    _external_led_index = value_to_index(GetHAL().getLeftExternalLedBrightness(), _external_led_levels.data(),
                                         _external_led_levels.size());
    _controls_syncing = true;
    lv_slider_set_value(_brightness_slider, _brightness_levels[_brightness_index], LV_ANIM_OFF);
    lv_slider_set_value(_volume_slider, _volume_levels[_volume_index], LV_ANIM_OFF);
    lv_slider_set_value(_external_led_slider, _external_led_levels[_external_led_index], LV_ANIM_OFF);
    update_controls_labels();
    _controls_syncing = false;

    lv_obj_clear_flag(_controls_scrim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_controls_scrim);
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

static void set_listen_indicator_requested(bool visible)
{
    std::lock_guard<std::mutex> lock(_llm_mutex);
    _listen_indicator_requested = visible;
}

static void set_mic_button_state_requested(MicButtonState state)
{
    std::lock_guard<std::mutex> lock(_llm_mutex);
    _mic_button_state_requested = state;
}

static void begin_xiaozhi_voice_input()
{
    const auto now = GetHAL().millis();
    if (_mic_button_event_at != 0 && now - _mic_button_event_at < 300) {
        return;
    }
    _mic_button_event_at = now;

    if (_mic_press_active || _xiaozhi_interaction_requested) {
        return;
    }

    if (_last_llm_request_at != 0 && now - _last_llm_request_at < 300) {
        publish_mqtt_state("mic.debounced", "ignored");
        return;
    }
    handle_mqtt_output_connection_watchdog(now);
    if (!is_mqtt_output_connected()) {
        set_listen_indicator_requested(false);
        set_mic_button_state_requested(MicButtonState::Idle);
        update_llm_status(GetHAL().getWifiStatus() == WifiStatus::None ? "Wi-Fiにつながってないの" : "Macに接続中だよ");
        publish_mqtt_state("mic.blocked", "relay_disconnected");
        return;
    }
    _last_llm_request_at = now;
    _audio_ws_requested = false;
    _audio_ws_close_requested = true;
    close_audio_ws_receiver();
    clear_pending_assistant_audio();
    Application::GetInstance().StopListening();
    _mic_press_active = true;
    _mic_pressed_at = now;
    _mic_touch_lost_at = 0;
    _xiaozhi_interaction_requested = true;
    {
        std::lock_guard<std::mutex> lock(_llm_mutex);
        _xiaozhi_listening_started = false;
        _xiaozhi_text_waiting = false;
        _xiaozhi_text_waiting_at = 0;
    }
    publish_mqtt_state("mic.pressed", "start");
    if (GetHAL().isXiaozhiSpeaking()) {
        set_mic_button_state_requested(MicButtonState::Starting);
        update_llm_status("キャンセル中");
    } else {
        set_mic_button_state_requested(MicButtonState::Starting);
        update_llm_status("マイク起動中");
    }
    GetHAL().requestXiaozhiListening();
}

static void end_xiaozhi_voice_input()
{
    if (!_mic_press_active) {
        return;
    }

    const auto now = GetHAL().millis();
    const bool too_short = _mic_pressed_at == 0 || now - _mic_pressed_at < _mic_min_hold_ms;
    bool listening_started = false;
    {
        std::lock_guard<std::mutex> lock(_llm_mutex);
        listening_started = _xiaozhi_listening_started;
    }

    _mic_press_active = false;
    _mic_touch_lost_at = 0;
    publish_mqtt_state("mic.released", "stop");
    if (too_short || !listening_started) {
        set_listen_indicator_requested(false);
        set_mic_button_state_requested(MicButtonState::Idle);
        update_llm_status(too_short ? "もう少し長押ししてね" : "準備できなかったの");
        {
            std::lock_guard<std::mutex> lock(_llm_mutex);
            _xiaozhi_listening_started = false;
            _xiaozhi_text_waiting = false;
            _xiaozhi_text_waiting_at = 0;
        }
        request_xiaozhi_stop_listening(too_short ? "short_press" : "not_listening");
        publish_mqtt_state("mic.cancelled", too_short ? "short_press" : "not_listening");
        _xiaozhi_interaction_requested = false;
        _mic_pressed_at = 0;
        _mic_touch_lost_at = 0;
        return;
    }

    set_mic_button_state_requested(MicButtonState::Starting);
    update_llm_status("送信中");
    {
        std::lock_guard<std::mutex> lock(_llm_mutex);
        _xiaozhi_text_waiting = true;
        _xiaozhi_text_waiting_at = GetHAL().millis();
    }
    request_xiaozhi_stop_listening("release");
    _xiaozhi_interaction_requested = false;
    _mic_pressed_at = 0;
    _mic_touch_lost_at = 0;
}

static void request_xiaozhi_stop_listening(const char* reason)
{
    publish_mqtt_state("xiaozhi.stop.requested", reason);
    Application::GetInstance().StopListening();
    GetHAL().stopXiaozhiListening();
}

static void handle_mic_button_event(lv_event_t* event)
{
    const auto code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) {
        begin_xiaozhi_voice_input();
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        end_xiaozhi_voice_input();
    }
}

static void publish_touch_point()
{
    const auto touch = hal_bridge::get_touch_point();
    const auto now = GetHAL().millis();
    if (touch.num > 0) {
        const bool moved = std::abs(touch.x - _last_touch_point_x) > 8 || std::abs(touch.y - _last_touch_point_y) > 8;
        if (!_touch_point_active || moved || now - _touch_point_published_at > 750) {
            char text[40];
            std::snprintf(text, sizeof(text), "x=%d y=%d", touch.x, touch.y);
            publish_mqtt_state("touch.point", text);
            _touch_point_active = true;
            _touch_point_published_at = now;
            _last_touch_point_x = touch.x;
            _last_touch_point_y = touch.y;
        }
    } else {
        _touch_point_active = false;
        _last_touch_point_x = -1;
        _last_touch_point_y = -1;
    }
}

static bool is_touch_inside_mic_button()
{
    if (!_touch_point_active) {
        return false;
    }

    constexpr int margin = 14;
    return _last_touch_point_x >= _top_mark_x_mic - margin &&
           _last_touch_point_x <= _top_mark_x_mic + _top_mark_size + margin &&
           _last_touch_point_y >= _top_mark_y - margin &&
           _last_touch_point_y <= _top_mark_y + _top_mark_size + margin;
}

static void handle_mic_touch_release_fallback()
{
    if (!_mic_press_active) {
        _mic_touch_lost_at = 0;
        return;
    }

    const auto now = GetHAL().millis();
    bool listening_started = false;
    {
        std::lock_guard<std::mutex> lock(_llm_mutex);
        listening_started = _xiaozhi_listening_started;
    }

    if (!listening_started) {
        _mic_touch_lost_at = 0;
        if (_mic_pressed_at != 0 && now - _mic_pressed_at >= _xiaozhi_start_timeout_ms) {
            _mic_press_active = false;
            _mic_pressed_at = 0;
            _xiaozhi_interaction_requested = false;
            set_listen_indicator_requested(false);
            set_mic_button_state_requested(MicButtonState::Idle);
            update_llm_status("起動に失敗したの");
            request_xiaozhi_stop_listening("start_timeout");
            publish_mqtt_state("mic.cancelled", "start_timeout");
        }
        return;
    }

    if (_mic_pressed_at != 0 && now - _mic_pressed_at < 150) {
        return;
    }

    if (is_touch_inside_mic_button()) {
        _mic_touch_lost_at = 0;
        return;
    }

    if (_mic_touch_lost_at == 0) {
        _mic_touch_lost_at = now;
        return;
    }

    if (now - _mic_touch_lost_at >= 120) {
        _mic_touch_lost_at = 0;
        end_xiaozhi_voice_input();
    }
}

static void handle_xiaozhi_start_timeout(uint32_t now)
{
    bool timed_out = false;
    {
        std::lock_guard<std::mutex> lock(_llm_mutex);
        if (!_xiaozhi_interaction_requested || _xiaozhi_listening_started || _mic_pressed_at == 0 ||
            now - _mic_pressed_at < _xiaozhi_start_timeout_ms) {
            return;
        }

        _listen_indicator_requested = false;
        _mic_button_state_requested = MicButtonState::Idle;
        _llm_status = "起動に失敗したの";
        _llm_status_changed = true;
        _caption_hide_requested = false;
        _mic_press_active = false;
        _mic_pressed_at = 0;
        _mic_touch_lost_at = 0;
        _xiaozhi_interaction_requested = false;
        _xiaozhi_listening_started = false;
        _xiaozhi_text_waiting = false;
        _xiaozhi_text_waiting_at = 0;
        timed_out = true;
    }

    if (timed_out) {
        request_xiaozhi_stop_listening("start_timeout");
        publish_mqtt_state("mic.cancelled", "start_timeout");
        mclog::tagWarn("NUKOEVI", "Xiaozhi start timed out");
    }
}

static void handle_xiaozhi_status(std::string_view status)
{
    publish_mqtt_state("xiaozhi.status", std::string(status));
    if (status == "Listening...") {
        std::lock_guard<std::mutex> lock(_llm_mutex);
        _xiaozhi_listening_started = true;
        _listen_indicator_requested = true;
        _mic_button_state_requested = MicButtonState::Listening;
        _llm_status = "聞いてるよ〜";
        _llm_status_changed = true;
        return;
    }

    if (status == "Connecting..." || status == "Logging in..." || status == "Loading assets..." ||
        status == "Activation") {
        set_listen_indicator_requested(false);
        if (_xiaozhi_interaction_requested) {
            set_mic_button_state_requested(MicButtonState::Starting);
            update_llm_status("マイク起動中");
        }
        return;
    }

    if (status == "Speaking...") {
        set_listen_indicator_requested(false);
        set_mic_button_state_requested(MicButtonState::Idle);
        return;
    }

    if (status == "Error") {
        std::lock_guard<std::mutex> lock(_llm_mutex);
        _listen_indicator_requested = false;
        _mic_button_state_requested = MicButtonState::Idle;
        if (_xiaozhi_interaction_requested || _mic_press_active || _xiaozhi_text_waiting) {
            _llm_status = "起動に失敗したの";
            _llm_status_changed = true;
            _caption_hide_requested = false;
        }
        _mic_press_active = false;
        _mic_pressed_at = 0;
        _mic_touch_lost_at = 0;
        _xiaozhi_interaction_requested = false;
        _xiaozhi_listening_started = false;
        _xiaozhi_text_waiting = false;
        _xiaozhi_text_waiting_at = 0;
        return;
    }

    if (status == "Standby") {
        std::lock_guard<std::mutex> lock(_llm_mutex);
        _listen_indicator_requested = false;
        _mic_button_state_requested = MicButtonState::Idle;
        if (_llm_status == "マイク起動中" || _llm_status == "キャンセル中" || _llm_status == "聞いてるよ〜") {
            _caption_hide_requested = true;
        }
        _xiaozhi_interaction_requested = false;
    }
}

static void handle_xiaozhi_text_message(const WsTextMessage_t& message)
{
    const std::string& role = message.name;
    const std::string& text = message.content;
    if (text.empty()) {
        return;
    }

    publish_mqtt_state("xiaozhi.text", text, role.c_str());
    if (role == "user") {
        {
            std::lock_guard<std::mutex> lock(_llm_mutex);
            _xiaozhi_listening_started = false;
            _xiaozhi_text_waiting = false;
            _xiaozhi_text_waiting_at = 0;
        }
        publish_mqtt_input(text, role.c_str());
    }

    std::lock_guard<std::mutex> lock(_llm_mutex);
    if (role == "assistant") {
        _llm_status = text;
        _llm_status_changed = true;
        return;
    }

    if (role == "user") {
        _llm_status = text;
        _llm_status_changed = true;
    }
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

static bool parse_bridge_response(std::string_view body, std::string& response)
{
    ArduinoJson::JsonDocument doc;
    auto error = ArduinoJson::deserializeJson(doc, body);
    if (error) {
        response.assign(body.data(), body.size());
        return !response.empty();
    }

    const char* text = doc["text"] | "";
    if (text[0] == '\0') {
        text = doc["message"] | "";
    }
    if (text[0] == '\0') {
        text = doc["detail"] | "";
    }
    response = text;
    return true;
}

static bool post_evictl_bridge_json(const ArduinoJson::JsonDocument& doc, std::string* response)
{
    std::string json;
    ArduinoJson::serializeJson(doc, json);
    if (json.empty()) {
        return false;
    }

    auto network = Board::GetInstance().GetNetwork();
    if (!network) {
        return false;
    }

    auto http = network->CreateHttp(0);
    if (!http) {
        return false;
    }

    http->SetHeader("Content-Type", "application/json");
    http->SetContent(std::move(json));
    if (!http->Open("POST", CONFIG_NUKOEVI_EVI_BRIDGE_ENDPOINT)) {
        return false;
    }

    const int status_code = http->GetStatusCode();
    std::string body = http->ReadAll();
    http->Close();
    if (status_code < 200 || status_code >= 300) {
        mclog::tagError("NUKOEVI", "evictl bridge http failed: status={}, body={}", status_code, body);
        return false;
    }

    if (response) {
        parse_bridge_response(body, *response);
    }
    return true;
}

static bool send_evictl_camera_capture(uint32_t request_id)
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
    if (!post_evictl_bridge_json(start_doc, nullptr)) {
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
        if (!post_evictl_bridge_json(chunk_doc, nullptr)) {
            free(jpeg_data);
            return false;
        }
        GetHAL().delay(15);
    }

    free(jpeg_data);
    return true;
}

static bool send_evictl_chat_prompt(uint32_t request_id, bool has_image, std::string& response)
{
    ArduinoJson::JsonDocument doc;
    doc["cmd"] = "chatPrompt";
    doc["data"]["id"] = request_id;
    doc["data"]["hasImage"] = has_image;
    doc["data"]["text"] = has_image ? "ｽﾀｯｸﾁｬﾝのカメラ画像を見て、ぬこエビちゃんとして短く答えて。見えているものを一言で教えて。"
                                    : "ぬこエビちゃんとして、短く挨拶して。";
    return post_evictl_bridge_json(doc, &response);
}

static void evictl_camera_task(void* arg)
{
    const uint32_t request_id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg));

    update_llm_status("カメラ撮影中");
    const bool has_image = send_evictl_camera_capture(request_id);
    update_llm_status("evictl送信中");

    std::string response;
    if (!send_evictl_chat_prompt(request_id, has_image, response)) {
        finish_llm_request(request_id, "evictl send failed");
        vTaskDelete(nullptr);
        return;
    }

    if (response.empty()) {
        response = "evictlへ送信しました";
    }
    finish_llm_request(request_id, response);

    vTaskDelete(nullptr);
}

static void begin_evictl_camera_task()
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

    if (GetHAL().getWifiStatus() == WifiStatus::None) {
        finish_llm_request(request_id, "Wi-Fi未接続");
        return;
    }

    const auto arg = reinterpret_cast<void*>(static_cast<uintptr_t>(request_id));
    if (xTaskCreate(evictl_camera_task, "nukoevi_evi", 12288, arg, 3, nullptr) != pdPASS) {
        finish_llm_request(request_id, "Task start failed");
    }
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
    _llm_status         = "応答がありません";
    _llm_status_changed = true;
    mclog::tagWarn("NUKOEVI", "LLM request timed out");
}

static void handle_xiaozhi_text_timeout(uint32_t now)
{
    bool timed_out = false;
    {
        std::lock_guard<std::mutex> lock(_llm_mutex);
        if (!_xiaozhi_text_waiting || _xiaozhi_text_waiting_at == 0 ||
            now - _xiaozhi_text_waiting_at < _xiaozhi_text_timeout_ms) {
            return;
        }

        _xiaozhi_text_waiting = false;
        _xiaozhi_text_waiting_at = 0;
        _xiaozhi_listening_started = false;
        _mic_pressed_at = 0;
        _mic_press_active = false;
        _xiaozhi_interaction_requested = false;
        _listen_indicator_requested = false;
        _mic_button_state_requested = MicButtonState::Idle;
        if (_llm_status == "送信中") {
            _llm_status = "聞き取れなかったの";
            _llm_status_changed = true;
        }
        timed_out = true;
    }

    if (timed_out) {
        request_xiaozhi_stop_listening("timeout");
        publish_mqtt_state("xiaozhi.text.timeout", "no transcript");
        mclog::tagWarn("NUKOEVI", "Xiaozhi text timed out");
    }
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

static void process_mqtt_output_messages()
{
    while (true) {
        std::string text;
        {
            std::lock_guard<std::mutex> lock(_mqtt_output_mutex);
            if (_mqtt_output_messages.empty()) {
                return;
            }
            text = _mqtt_output_messages.front();
            _mqtt_output_messages.pop();
        }
        update_caption_text(text);
        _caption_hide_after_audio = true;
        begin_caption_audio_wait(GetHAL().millis());
    }
}

static void hide_caption_panel()
{
    if (!_caption_panel || !_caption_visible) {
        return;
    }

    lv_obj_add_flag(_caption_panel, LV_OBJ_FLAG_HIDDEN);
    _caption_visible = false;
    _caption_hide_after_audio = false;
    reset_caption_audio_state();
    mclog::tagInfo("NUKOEVI", "caption hidden");
}

static void update_listen_indicator(bool visible)
{
    if (!_listen_indicator) {
        return;
    }
    if (_listen_indicator_visible == visible) {
        return;
    }

    if (visible) {
        lv_obj_clear_flag(_listen_indicator, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(_listen_indicator);
    } else {
        lv_obj_add_flag(_listen_indicator, LV_OBJ_FLAG_HIDDEN);
    }
    _listen_indicator_visible = visible;
}

static void handle_caption_auto_hide(uint32_t now)
{
    if (!_caption_panel || !_caption_visible || _llm_running || is_audio_playback_active(now) ||
        is_caption_hold_active(now)) {
        return;
    }

    if (_caption_hide_after_audio) {
        if (is_caption_waiting_for_audio(now)) {
            return;
        }
        hide_caption_panel();
        return;
    }

    if (now - _caption_updated_at < _caption_auto_hide_ms) {
        return;
    }

    hide_caption_panel();
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
            set_avatar_motion_frame(get_blink_motion_frame(0), "blink", 0);
            GetHAL().setExternalLedBrightness(_external_led_normal_brightness, _external_led_normal_brightness, false);
            mclog::tagInfo("NUKOEVI", "sleep mode exit");
        }
        return false;
    }

    if (!_sleep_mode) {
        _sleep_mode      = true;
        _sleep_index     = 0;
        _sleep_timecount = now;
        set_avatar_motion_frame(get_sleep_motion_frame(_sleep_index), "sleep", _sleep_index);
        GetHAL().setExternalLedBrightness(_external_led_sleep_brightness, _external_led_sleep_brightness, false);
        mclog::tagInfo("NUKOEVI", "sleep mode enter");
        return true;
    }

    const uint32_t interval = _sleep_intervals[_sleep_index];
    if (now - _sleep_timecount >= interval) {
        _sleep_timecount = now;
        _sleep_index++;
        if (_sleep_index >= _sleep_frame_count) {
            _sleep_index = 0;
        }
        set_avatar_motion_frame(get_sleep_motion_frame(_sleep_index), "sleep", _sleep_index);
    }

    return true;
}

AppNukoevi::AppNukoevi()
{
    setAppInfo().name = "NUKOEVI";
    static auto icon = assets::get_image("nukoevi_icon.bin");
    setAppInfo().icon = (void*)&icon;

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
    GetHAL().setBackLightBrightness(_nukoevi_backlight_brightness, false);
    GetHAL().setSpeakerVolume(_nukoevi_volume, false);
    if (_espnow_signal_connection < 0) {
        _espnow_signal_connection = GetHAL().onEspNowData.connect([](const std::vector<uint8_t>& data) {
            if (!_espnow_receives) {
                return;
            }

            std::lock_guard<std::mutex> lock(_espnow_mutex);
            _espnow_received_data = data;
        });
    }
    if (_xiaozhi_status_signal_connection < 0) {
        _xiaozhi_status_signal_connection = GetHAL().onXiaozhiStatus.connect(handle_xiaozhi_status);
    }
    if (_xiaozhi_text_signal_connection < 0) {
        _xiaozhi_text_signal_connection = GetHAL().onXiaozhiTextMessage.connect(handle_xiaozhi_text_message);
    }
    GetHAL().initExternalLedPwm();
    GetHAL().setExternalLedBrightness(_external_led_normal_brightness, _external_led_normal_brightness, false);
    start_network_once();
    GetHAL().startXiaozhiBackground();
    _espnow_receives = _enable_espnow_remote;
    if (_enable_espnow_remote && !_espnow_started) {
        GetHAL().startEspNow(1);
        _espnow_started = true;
        mclog::tagInfo(getAppInfo().name, "ESP-NOW remote receiver ready on channel 1, id 1");
    }
    LvglLockGuard lock;

    _panel = std::make_unique<Container>(lv_screen_active());
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(320, 240);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setBgColor(lv_color_hex(0xF5B06F));
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _panel->setPadding(0, 0, 0, 0);
    _panel->addFlag(LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_panel->get(), [](lv_event_t*) { show_controls_modal(); }, LV_EVENT_LONG_PRESSED, nullptr);

    load_blink_motion_assets();
    load_sleep_motion_assets();

    _avatar = std::make_unique<Image>(_panel->get());
    _avatar->setSrc(get_blink_motion_frame(0));
    _avatar->align(LV_ALIGN_CENTER, 0, 0);
    _avatar->addFlag(LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_avatar->get(), [](lv_event_t*) { show_controls_modal(); }, LV_EVENT_LONG_PRESSED, nullptr);
    _blink_index     = 0;
    _blink_timecount = GetHAL().millis();
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
    lv_obj_add_flag(_caption_panel, LV_OBJ_FLAG_HIDDEN);
    _caption_visible = false;

    _listen_indicator = lv_obj_create(lv_screen_active());
    lv_obj_set_size(_listen_indicator, 18, 18);
    lv_obj_align(_listen_indicator, LV_ALIGN_TOP_LEFT, _top_mark_x_mic + 35, _top_mark_y + 35);
    lv_obj_set_style_radius(_listen_indicator, 9, LV_PART_MAIN);
    lv_obj_set_style_border_width(_listen_indicator, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(_listen_indicator, lv_color_hex(0xFFF4E6), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_listen_indicator, lv_color_hex(0x2B1710), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_listen_indicator, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_listen_indicator, 0, LV_PART_MAIN);
    lv_obj_clear_flag(_listen_indicator, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(_listen_indicator, LV_OBJ_FLAG_CLICKABLE);

    _listen_indicator_dot = lv_obj_create(_listen_indicator);
    lv_obj_set_size(_listen_indicator_dot, 6, 6);
    lv_obj_align(_listen_indicator_dot, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(_listen_indicator_dot, 3, LV_PART_MAIN);
    lv_obj_set_style_border_width(_listen_indicator_dot, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_listen_indicator_dot, lv_color_hex(0xF5B06F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_listen_indicator_dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(_listen_indicator_dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_listen_indicator, LV_OBJ_FLAG_HIDDEN);
    _listen_indicator_visible = false;

    create_top_controls();

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
    if (open_home_if_requested()) {
        close();
        return;
    }

    if (open_setup_wifi_if_requested()) {
        return;
    }

    LvglLockGuard lock;
    view::update_home_indicator();
    handle_espnow_remote_motion();
    handle_head_pet_motion();
    GetStackChan().update();
    update_wifi_button();
    update_battery_indicator();
    const auto now                      = GetHAL().millis();
    handle_mqtt_output_connection_watchdog(now);
    process_mqtt_output_messages();
    ensure_audio_ws_receiver();
    publish_touch_point();
    handle_mic_touch_release_fallback();

    constexpr uint32_t blink_interval_ms = 3200;
    constexpr uint32_t blink_frame_ms    = 85;
    handle_llm_timeout(now);
    handle_xiaozhi_start_timeout(now);
    handle_xiaozhi_text_timeout(now);
    handle_caption_auto_hide(now);

    if (!_avatar) {
        return;
    }

    bool should_start_llm = false;
    bool should_hide_caption = false;
    bool listen_indicator_visible = false;
    MicButtonState mic_button_state = MicButtonState::Idle;
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
        if (_caption_hide_requested) {
            _caption_hide_requested = false;
            should_hide_caption = true;
        }
        listen_indicator_visible = _listen_indicator_requested;
        mic_button_state = _mic_button_state_requested;
    }

    update_mic_button(mic_button_state);
    update_listen_indicator(listen_indicator_visible);
    if (should_hide_caption && !is_audio_playback_active(now) && !is_caption_hold_active(now)) {
        hide_caption_panel();
    }

    if (should_start_llm) {
        begin_local_llm_task();
    }

    if (update_sleep_animation(now)) {
        return;
    }

    if (_blink_index == 0) {
        if (now - _blink_timecount >= blink_interval_ms) {
            _blink_index     = 1;
            _blink_timecount = now;
            set_avatar_motion_frame(get_blink_motion_frame(_blink_sequence_indices[_blink_index]), "blink", _blink_index);
        }
        return;
    }

    if (now - _blink_timecount >= blink_frame_ms) {
        _blink_index++;
        if (_blink_index >= _blink_sequence_count) {
            _blink_index = 0;
        }
        _blink_timecount = now;
        set_avatar_motion_frame(get_blink_motion_frame(_blink_sequence_indices[_blink_index]), "blink", _blink_index);
    }
}

void AppNukoevi::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    LvglLockGuard lock;

    view::destroy_home_indicator();
    _head_pet_receives = false;
    _espnow_receives = false;
    if (_xiaozhi_status_signal_connection >= 0) {
        GetHAL().onXiaozhiStatus.disconnect(_xiaozhi_status_signal_connection);
        _xiaozhi_status_signal_connection = -1;
    }
    if (_xiaozhi_text_signal_connection >= 0) {
        GetHAL().onXiaozhiTextMessage.disconnect(_xiaozhi_text_signal_connection);
        _xiaozhi_text_signal_connection = -1;
    }
    {
        std::lock_guard<std::mutex> lock(_espnow_mutex);
        _espnow_received_data.clear();
    }
    GetHAL().onBleConfigData.clear();
    hide_controls_modal();
    if (_controls_scrim && lv_obj_is_valid(_controls_scrim)) {
        lv_obj_delete(_controls_scrim);
    }
    if (_mic_button && lv_obj_is_valid(_mic_button)) {
        lv_obj_delete(_mic_button);
    }
    if (_camera_button && lv_obj_is_valid(_camera_button)) {
        lv_obj_delete(_camera_button);
    }
    if (_wifi_button && lv_obj_is_valid(_wifi_button)) {
        lv_obj_delete(_wifi_button);
    }
    if (_home_button && lv_obj_is_valid(_home_button)) {
        lv_obj_delete(_home_button);
    }
    if (_battery_panel && lv_obj_is_valid(_battery_panel)) {
        lv_obj_delete(_battery_panel);
    }
    if (_caption_panel && lv_obj_is_valid(_caption_panel)) {
        lv_obj_delete(_caption_panel);
    }
    if (_listen_indicator && lv_obj_is_valid(_listen_indicator)) {
        lv_obj_delete(_listen_indicator);
    }
    _mic_button = nullptr;
    _camera_button = nullptr;
    _camera_label = nullptr;
    _wifi_button = nullptr;
    _wifi_label = nullptr;
    _wifi_off_badge = nullptr;
    _home_button = nullptr;
    _home_label = nullptr;
    _battery_panel = nullptr;
    _battery_label = nullptr;
    _controls_scrim = nullptr;
    _controls_modal = nullptr;
    _brightness_label = nullptr;
    _brightness_slider = nullptr;
    _volume_label = nullptr;
    _volume_slider = nullptr;
    _external_led_label = nullptr;
    _external_led_slider = nullptr;
    _llm_label     = nullptr;
    _caption_panel = nullptr;
    _listen_indicator = nullptr;
    _listen_indicator_dot = nullptr;
    {
        std::lock_guard<std::mutex> lock(_audio_ws_mutex);
        if (_audio_ws_client) {
            _audio_ws_client->Close();
            _audio_ws_client.reset();
        }
    }
    _audio_ws_connecting = false;
    _mqtt_output_connected = false;
    _mqtt_output_disconnected_at = 0;
    _caption_visible = false;
    _caption_hide_after_audio = false;
    _caption_updated_at = 0;
    reset_caption_audio_state();
    _listen_indicator_visible = false;
    _listen_indicator_requested = false;
    _mic_button_state_visible = MicButtonState::Idle;
    _mic_button_state_requested = MicButtonState::Idle;
    _caption_hide_requested = false;
    _mic_press_active = false;
    _mic_pressed_at = 0;
    _mic_touch_lost_at = 0;
    _xiaozhi_interaction_requested = false;
    _xiaozhi_listening_started = false;
    _xiaozhi_text_waiting = false;
    _xiaozhi_text_waiting_at = 0;
    _open_home_requested = false;
    _avatar.reset();
    _panel.reset();
}
