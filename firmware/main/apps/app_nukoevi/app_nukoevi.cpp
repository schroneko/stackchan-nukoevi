#include "app_nukoevi.h"
#include <apps/common/common.h>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <cstdint>
#include <memory>
#include <smooth_lvgl.hpp>

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

    view::create_home_indicator([&]() { close(); }, 0xF8C489, 0x4A241A, lv_screen_active());
}

void AppNukoevi::onRunning()
{
    LvglLockGuard lock;
    view::update_home_indicator();

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
    _avatar.reset();
    _panel.reset();
}
