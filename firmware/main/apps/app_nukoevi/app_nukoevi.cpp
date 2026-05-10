#include "app_nukoevi.h"
#include <apps/common/common.h>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <memory>
#include <smooth_lvgl.hpp>

using namespace mooncake;
using namespace smooth_ui_toolkit::lvgl_cpp;

LV_IMAGE_DECLARE(nukoevi_icon);
LV_IMAGE_DECLARE(nukoevi_screen);

static std::unique_ptr<Container> _panel;
static std::unique_ptr<Image> _avatar;

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
    _avatar->setSrc(&nukoevi_screen);
    _avatar->align(LV_ALIGN_CENTER, 0, 0);

    view::create_home_indicator([&]() { close(); }, 0xF8C489, 0x4A241A, lv_screen_active());
}

void AppNukoevi::onRunning()
{
    LvglLockGuard lock;
    view::update_home_indicator();
}

void AppNukoevi::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    LvglLockGuard lock;

    view::destroy_home_indicator();
    _avatar.reset();
    _panel.reset();
}
