/**
 * @file gpioController.cpp
 * @brief C++ class implementation
 */

#include "gpioController.hpp"   // C++ class definition
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "driver/ledc.h"

namespace bsw {
/* ---------------- Constant Definitions ---------------- */
const uint16_t GpioController::pwm_lookup_table[kPwmLookupTableSize] = {
    0, 10, 20, 30, 40, 51, 61, 71, 81, 91, 102, 112, 122, 132, 143, 153, 163, 173, 184, 194,
    204, 214, 225, 235, 245, 256, 266, 276, 286, 297, 307, 317, 328, 338, 348, 358, 369, 379,
    389, 400, 410, 420, 430, 441, 451, 461, 471, 482, 492, 502, 513, 523, 533, 543, 554, 564,
    574, 584, 595, 605, 615, 625, 636, 646, 656, 666, 677, 687, 697, 708, 718, 728, 738, 749, 759,
    769, 779, 790, 800, 810, 821, 831, 841, 851, 862, 872, 882, 892, 903, 913, 923, 934, 944,
    954, 964, 975, 985, 995, 1005, 1016, 1024
};

/* ---------------- C++ class ---------------- */
GpioController::GpioController() {}
void GpioController::init() {}

void GpioController::setDirection(uint8_t gpio_id, GpioDirection direction)
{
    gpio_set_direction(static_cast<gpio_num_t>(gpio_id), 
                      (direction == GpioDirection::kOutput) ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
}

void GpioController::setPullMode(uint8_t gpio_id, GpioPullMode pull_mode)
{
    gpio_pull_mode_t pull_mode_esp {static_cast<gpio_pull_mode_t>(GPIO_FLOATING)};
    switch (pull_mode)
    {
    case GpioPullMode::kPullUp:
        pull_mode_esp = GPIO_PULLUP_ONLY;
        break;
    case GpioPullMode::kPullDown:
        pull_mode_esp = GPIO_PULLDOWN_ONLY;
        break;
    case GpioPullMode::kPullUpDown:
        pull_mode_esp = GPIO_PULLUP_PULLDOWN;
        break;
    default: // already set as floating
        break;
    }
    gpio_set_pull_mode(static_cast<gpio_num_t>(gpio_id),
                       pull_mode_esp);
}

void GpioController::setGpioState(uint8_t gpio_id, GpioState state)
{
    gpio_set_level(static_cast<gpio_num_t>(gpio_id),
                   (state == GpioState::kHigh) ? 1 : 0);
}

void GpioController::initPwm(uint8_t gpio_id, uint32_t frequency, uint8_t duty_cycle, uint8_t channel, uint8_t timer)
{
    ledc_channel_config_t ledc_channel = {0};
    ledc_channel.gpio_num = gpio_id;
    ledc_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_channel.channel = static_cast<ledc_channel_t>(channel);
    ledc_channel.intr_type = LEDC_INTR_DISABLE;
    ledc_channel.timer_sel = static_cast<ledc_timer_t>(timer);
    ledc_channel.duty = getPwmDutyCycleLookup(duty_cycle);

    /* Initialize PWM timer if it hasn't been initialized yet */
	if (pwm_timer_initialized == false)
	{
		ledc_timer_config_t ledc_timer = {0};
		ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE;
		ledc_timer.duty_resolution = LEDC_TIMER_10_BIT;
		ledc_timer.timer_num = static_cast<ledc_timer_t>(timer);
		ledc_timer.freq_hz = frequency;
		ESP_ERROR_CHECK( ledc_timer_config(&ledc_timer) );
		pwm_timer_initialized = true;
	}
}

GpioState GpioController::getState(uint8_t gpio_id) const
{
    // Return current GPIO state
    int level = gpio_get_level(static_cast<gpio_num_t>(gpio_id));
    return (level == 1) ? GpioState::kHigh : GpioState::kLow;
}

void GpioController::setPwmDuty(uint8_t channel, uint8_t duty_cycle)
{
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, static_cast<ledc_channel_t>(channel), getPwmDutyCycleLookup(duty_cycle));
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, static_cast<ledc_channel_t>(channel));
}

void GpioController::setPwmFreq(uint8_t channel, uint32_t frequency)
{
    ledc_set_freq(LEDC_HIGH_SPEED_MODE, static_cast<ledc_timer_t>(channel), frequency);
}

void GpioController::stopPwm(uint8_t channel)
{
    ledc_stop(LEDC_HIGH_SPEED_MODE, static_cast<ledc_channel_t>(channel), 0);
}

} // namespace bsw