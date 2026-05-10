#include "app_nukoevi.h"
#include <apps/common/common.h>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <stackchan/stackchan.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <smooth_lvgl.hpp>
#include <vector>

using namespace mooncake;
using namespace smooth_ui_toolkit::lvgl_cpp;

LV_IMAGE_DECLARE(nukoevi_icon);
LV_IMAGE_DECLARE(nukoevi_screen_open);
LV_IMAGE_DECLARE(nukoevi_screen_half_a);
LV_IMAGE_DECLARE(nukoevi_screen_closed);
LV_IMAGE_DECLARE(nukoevi_screen_half_b);

static std::unique_ptr<Container> _panel;
static std::unique_ptr<Image> _avatar;
static uint32_t _blink_timecount = 0;
static uint8_t _blink_index      = 0;
static std::mutex _espnow_mutex;
static std::vector<uint8_t> _espnow_data;
static bool _espnow_started  = false;
static bool _espnow_receives = false;
static constexpr uint8_t _espnow_receiver_id = 1;
static constexpr int _espnow_wifi_channel    = 1;
static bool _head_pet_receives                = false;
static int _head_pet_signal_connection        = -1;
static volatile bool _head_pet_requested      = false;
static bool _head_pet_active                  = false;
static uint8_t _head_pet_step                 = 0;
static uint32_t _head_pet_timecount           = 0;
static int _head_pet_base_yaw                 = 0;
static int _head_pet_base_pitch               = 0;

static const lv_image_dsc_t* const _blink_sequence[] = {
    &nukoevi_screen_open,
    &nukoevi_screen_half_a,
    &nukoevi_screen_closed,
    &nukoevi_screen_half_b,
    &nukoevi_screen_open,
};

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

    if (!_espnow_started) {
        GetHAL().startEspNow(_espnow_wifi_channel);
        GetHAL().onEspNowData.connect([](const std::vector<uint8_t>& data) {
            if (!_espnow_receives) {
                return;
            }

            std::lock_guard<std::mutex> lock(_espnow_mutex);
            _espnow_data = data;
        });
        _espnow_started = true;
    }
    _espnow_receives = true;

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

    view::create_home_indicator([&]() { close(); }, 0xF8C489, 0x4A241A, lv_screen_active());
}

static void handle_espnow_remote()
{
    std::lock_guard<std::mutex> lock(_espnow_mutex);

    if (_espnow_data.size() < 8) {
        _espnow_data.clear();
        return;
    }

    const uint8_t target_id = _espnow_data[0];
    if (target_id != 0 && target_id != _espnow_receiver_id) {
        _espnow_data.clear();
        return;
    }

    const int16_t yaw_angle   = static_cast<int16_t>(_espnow_data[1] | (_espnow_data[2] << 8));
    const int16_t pitch_angle = static_cast<int16_t>(_espnow_data[3] | (_espnow_data[4] << 8));
    const int16_t speed       = static_cast<int16_t>(_espnow_data[5] | (_espnow_data[6] << 8));
    const bool laser_enabled  = (_espnow_data[7] != 0);

    mclog::tagInfo("NUKOEVI", "espnow remote size {}, target {}, yaw {}, pitch {}, speed {}, laser {}",
                   _espnow_data.size(), target_id, yaw_angle, pitch_angle, speed, laser_enabled);

    GetStackChan().motion().moveWithSpeed(yaw_angle, pitch_angle, speed);
    GetHAL().setLaserEnabled(laser_enabled);

    _espnow_data.clear();
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
    handle_espnow_remote();
    handle_head_pet_motion();

    constexpr uint32_t blink_interval_ms = 3200;
    constexpr uint32_t blink_frame_ms    = 85;
    const auto now                      = GetHAL().millis();

    if (!_avatar) {
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
    _espnow_receives = false;
    _head_pet_receives = false;
    _avatar.reset();
    _panel.reset();
}
