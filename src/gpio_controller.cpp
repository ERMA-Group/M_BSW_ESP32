/**
 * @file gpio_controller.cpp
 * @brief C++ class implementation
 */

#include "gpio_controller.hpp"   // C++ class definition
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

/* PWM LED brightness lookup table */
const uint16_t GpioController::pwm_brightness_lookup_table[kPwmLookupTableSize] = {
    0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 6, 7, 7, 8,
    9, 10, 11, 12, 12, 13, 15, 17, 19, 21, 23, 26, 29, 32, 35, 38, 42, 46, 50, 54,
    59, 64, 69, 74, 80, 86, 93, 100, 107, 115, 123, 131, 140, 149, 159, 169, 179, 190, 201, 213,
    225, 238, 251, 264, 278, 293, 308, 323, 339, 356, 373, 390, 408, 427, 446, 465, 485, 506, 527, 549,
    571, 594, 617, 641, 665, 690, 716, 742, 769, 796, 824, 852, 881, 911, 941, 972, 1003, 1024
};

const uint16_t GpioController::pwm_brightness_lookup_table_inv[kPwmLookupTableSize] = {
    1024, 1024, 1024, 1024, 1024, 1024, 1023, 1023, 1023, 1022, 1022, 1021, 1021, 1020, 1020, 1019, 1018, 1017, 1017, 1016,
    1015, 1014, 1013, 1012, 1012, 1011, 1009, 1007, 1005, 1003, 1001, 998, 995, 992, 989, 986, 982, 978, 974, 970,
    965, 960, 955, 950, 944, 938, 931, 924, 917, 909, 901, 893, 884, 875, 865, 855, 845, 834, 823, 811,
    799, 786, 773, 760, 746, 731, 716, 701, 685, 668, 651, 634, 616, 597, 578, 559, 539, 518, 497, 475,
    453, 430, 407, 383, 359, 334, 308, 282, 255, 228, 200, 172, 143, 113, 83, 52, 21, 0
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
		ledc_timer_config_t ledc_timer = {};
		ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE;
		ledc_timer.duty_resolution = LEDC_TIMER_10_BIT;
		ledc_timer.timer_num = static_cast<ledc_timer_t>(timer);
		ledc_timer.freq_hz = frequency;
		ledc_timer.clk_cfg = LEDC_AUTO_CLK;
		ESP_ERROR_CHECK( ledc_timer_config(&ledc_timer) );
		pwm_timer_initialized = true;
	}
    ledc_channel_config(&ledc_channel);
}

GpioState GpioController::getState(uint8_t gpio_id) const
{
    // Return current GPIO state
    auto level {gpio_get_level(static_cast<gpio_num_t>(gpio_id))};
    return (level == 1) ? GpioState::kHigh : GpioState::kLow;
}

/**
 * Set PWM duty cycle for a specific channel.
 * @param channel The PWM channel to set the duty cycle for.
 * @param duty_cycle The desired duty cycle (0-100).
 * @param is_brightness If true, use brightness lookup table; otherwise, use duty cycle lookup table.
 * @param pwm_backwards If true, invert the PWM signal.
 */
void GpioController::setPwmDuty(uint8_t channel, uint8_t duty_cycle, bool is_brightness, bool pwm_inverted)
{
    if (is_brightness)
    {
        if (pwm_inverted)
        {
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, static_cast<ledc_channel_t>(channel), getPwmBrightnessLookupInv(duty_cycle));
        }
        else
        {
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, static_cast<ledc_channel_t>(channel), getPwmBrightnessLookup(duty_cycle));
        }
    }
    else
    {
        if (pwm_inverted)
        {
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, static_cast<ledc_channel_t>(channel), 1024 - getPwmDutyCycleLookup(duty_cycle));
        }
        else
        {
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, static_cast<ledc_channel_t>(channel), getPwmDutyCycleLookup(duty_cycle));
        }
    }
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

uint16_t GpioController::getPwmDutyCycleLookup(uint8_t duty_cycle) const
{
    if (duty_cycle >= kPwmLookupTableSize)
    {
        duty_cycle = kPwmLookupTableSize - 1;
    }
    return pwm_lookup_table[duty_cycle];
}

uint16_t GpioController::getPwmBrightnessLookup(uint8_t brightness) const
{
    if (brightness >= kPwmLookupTableSize)
    {
        brightness = kPwmLookupTableSize - 1;
    }
    return pwm_brightness_lookup_table[brightness];
}

uint16_t GpioController::getPwmBrightnessLookupInv(uint8_t brightness) const
{
    if (brightness >= kPwmLookupTableSize)
    {
        brightness = kPwmLookupTableSize - 1;
    }
    return pwm_brightness_lookup_table_inv[brightness];
}

} // namespace bsw