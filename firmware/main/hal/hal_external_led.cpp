#include "hal.h"
#include <driver/ledc.h>
#include <mooncake_log.h>

static const std::string_view _tag = "HAL-EXT-LED";
static constexpr ledc_mode_t _speed_mode = LEDC_LOW_SPEED_MODE;
static constexpr ledc_timer_t _timer = LEDC_TIMER_3;
static constexpr ledc_timer_bit_t _resolution = LEDC_TIMER_10_BIT;
static constexpr uint32_t _frequency_hz = 5000;
static constexpr uint32_t _max_duty = 1023;
static constexpr ledc_channel_t _left_channel = LEDC_CHANNEL_6;
static constexpr ledc_channel_t _right_channel = LEDC_CHANNEL_7;
static constexpr bool _pwm_inverted =
#ifdef CONFIG_NUKOEVI_EXTERNAL_LED_PWM_INVERTED
    true;
#else
    false;
#endif
static uint8_t _left_brightness = 100;
static uint8_t _right_brightness = 100;
static bool _external_led_pwm_ready = false;

static uint8_t clamp_percent(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return static_cast<uint8_t>(value);
}

static bool valid_gpio(int gpio)
{
    return gpio >= 0 && gpio <= 48;
}

static uint32_t percent_to_duty(uint8_t percent)
{
    const auto duty = static_cast<uint32_t>(percent) * _max_duty / 100;
    return _pwm_inverted ? _max_duty - duty : duty;
}

static void configure_channel(ledc_channel_t channel, int gpio, uint8_t brightness)
{
    if (!valid_gpio(gpio)) {
        return;
    }

    ledc_channel_config_t config = {};
    config.gpio_num = gpio;
    config.speed_mode = _speed_mode;
    config.channel = channel;
    config.intr_type = LEDC_INTR_DISABLE;
    config.timer_sel = _timer;
    config.duty = percent_to_duty(brightness);
    config.hpoint = 0;
    ledc_channel_config(&config);
}

static void set_channel_duty(ledc_channel_t channel, int gpio, uint8_t brightness)
{
    if (!valid_gpio(gpio)) {
        return;
    }

    ledc_set_duty(_speed_mode, channel, percent_to_duty(brightness));
    ledc_update_duty(_speed_mode, channel);
}

void Hal::initExternalLedPwm()
{
    if (_external_led_pwm_ready) {
        return;
    }

    _left_brightness = 30;
    _right_brightness = 30;

    ledc_timer_config_t timer_config = {};
    timer_config.speed_mode = _speed_mode;
    timer_config.duty_resolution = _resolution;
    timer_config.timer_num = _timer;
    timer_config.freq_hz = _frequency_hz;
    timer_config.clk_cfg = LEDC_AUTO_CLK;
    if (ledc_timer_config(&timer_config) != ESP_OK) {
        mclog::tagError(_tag, "timer config failed");
        return;
    }

    configure_channel(_left_channel, CONFIG_NUKOEVI_EXTERNAL_LED_LEFT_GPIO, _left_brightness);
    configure_channel(_right_channel, CONFIG_NUKOEVI_EXTERNAL_LED_RIGHT_GPIO, _right_brightness);
    _external_led_pwm_ready = true;
    mclog::tagInfo(_tag, "external LED PWM ready left_gpio={} right_gpio={} inverted={}",
                   CONFIG_NUKOEVI_EXTERNAL_LED_LEFT_GPIO, CONFIG_NUKOEVI_EXTERNAL_LED_RIGHT_GPIO,
                   _pwm_inverted);
}

void Hal::setExternalLedBrightness(uint8_t left, uint8_t right, bool permanent)
{
    if (!_external_led_pwm_ready) {
        initExternalLedPwm();
    }

    _left_brightness = clamp_percent(left);
    _right_brightness = clamp_percent(right);
    set_channel_duty(_left_channel, CONFIG_NUKOEVI_EXTERNAL_LED_LEFT_GPIO, _left_brightness);
    set_channel_duty(_right_channel, CONFIG_NUKOEVI_EXTERNAL_LED_RIGHT_GPIO, _right_brightness);
}

uint8_t Hal::getLeftExternalLedBrightness()
{
    return _left_brightness;
}

uint8_t Hal::getRightExternalLedBrightness()
{
    return _right_brightness;
}
