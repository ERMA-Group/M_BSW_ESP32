/**
 * @file gpio.cpp
 * @brief C++ class implementation
 */

#include "gpio.hpp"   // C++ class definition

/* ---------------- C++ class ---------------- */
Gpio::Gpio(GpioController& gpio_controller, uint8_t gpio_id, GpioDirection direction, GpioPullMode pull_mode, GpioState initial_state)
    : gpio_controller(gpio_controller),
      gpio_id(gpio_id),
      direction(direction),
      pull_mode(pull_mode),
      state(initial_state),
      mode(GpioMode::kDigital),
      pwm_channel(0),
      pwm_timer(0),
      pwm_frequency(0),
      pwm_duty_cycle(0)
{
    // Initialize GPIO with specified parameters
}

void Gpio::initPwm(uint8_t gpio_id, uint32_t frequency, uint8_t duty_cycle, uint8_t channel, uint8_t timer)
{
    gpio_controller.initPwm(gpio_id, frequency, duty_cycle, channel, timer);
    mode = GpioMode::kPwm;
    pwm_channel = channel;
    pwm_timer = timer;
    pwm_frequency = frequency;
    pwm_duty_cycle = duty_cycle;
}

void Gpio::setDirection(uint8_t gpio_id, GpioDirection direction)
{
    gpio_controller.setDirection(gpio_id, direction);
}

void Gpio::setPullMode(uint8_t gpio_id, GpioPullMode pull_mode)
{
    gpio_controller.setPullMode(gpio_id, pull_mode);
}
void Gpio::setGpioState(uint8_t gpio_id, GpioState state)
{
    gpio_controller.setGpioState(gpio_id, state);
}

void Gpio::setPwmDuty(uint8_t gpio_id, uint8_t duty_cycle)
{
    // Set PWM duty cycle
}

void Gpio::setPwmFreq(uint8_t gpio_id, uint32_t frequency)
{
    // Set PWM frequency
}

GpioState Gpio::getState(uint8_t gpio_id) const
{
    return gpio_controller.getState(gpio_id);
}

GpioMode Gpio::getMode() const
{
    return mode;
}

uint8_t Gpio::getPwmChannel() const
{
    return pwm_channel;
}

uint8_t Gpio::getPwmTimer() const
{
    return pwm_timer;
}

uint32_t Gpio::getPwmFrequency() const
{
    return pwm_frequency;
}

uint8_t Gpio::getPwmDutyCycle() const
{
    return pwm_duty_cycle;
}