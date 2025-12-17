/**
 * @file gpio.cpp
 * @brief C++ class implementation
 */

#include "gpio.hpp"   // C++ class definition
#include <cstdint>
#include "esp_timer.h"
namespace bsw {

/* ---------------- C++ class ---------------- */
Gpio::Gpio(GpioController& in_gpio_controller, const uint8_t in_gpio_id, const GpioDirection in_direction, const GpioPullMode in_pull_mode, const GpioState in_initial_state) noexcept
    : _gpio_controller(in_gpio_controller),
      _gpio_id(in_gpio_id),
      _direction(in_direction),
      _pull_mode(in_pull_mode),
      _state(in_initial_state),
      _mode(GpioMode::kDigital),
      _pwm_channel(0),
      _pwm_timer(0),
      _pwm_frequency(0),
      _pwm_duty_cycle(0)
{
    // Initialize GPIO with specified parameters
}

void Gpio::init() noexcept
{
    _gpio_controller.setDirection(_gpio_id, _direction);
    _gpio_controller.setPullMode(_gpio_id, _pull_mode);
    _gpio_controller.setGpioState(_gpio_id, _state);
}

void Gpio::initPwm(const uint32_t frequency, const uint8_t duty_cycle, const uint8_t channel, const uint8_t timer) noexcept
{
    _gpio_controller.initPwm(_gpio_id, frequency, duty_cycle, channel, timer);
    _mode = GpioMode::kPwm;
    _pwm_channel = channel;
    _pwm_timer = timer;
    _pwm_frequency = frequency;
    _pwm_duty_cycle = duty_cycle;
}

void Gpio::setGpioId(const uint8_t gpio_id) noexcept
{
    this->_gpio_id = gpio_id;
}

void Gpio::setDirection(const GpioDirection direction) noexcept
{
    _direction = direction;
    _gpio_controller.setDirection(_gpio_id, direction);
}

void Gpio::setPullMode(const GpioPullMode pull_mode) noexcept
{
    _pull_mode = pull_mode;
    _gpio_controller.setPullMode(_gpio_id, pull_mode);
}
void Gpio::setState(const GpioState state) noexcept
{
    _state = state;
    _gpio_controller.setGpioState(_gpio_id, state);
}

void Gpio::toggleGpioState() noexcept
{
    GpioState current_state = getState();
    GpioState new_state = (current_state == GpioState::kHigh) ? GpioState::kLow : GpioState::kHigh;
    setState(new_state);
}

void Gpio::setPwmDuty(const uint8_t duty_cycle) noexcept
{
    // Set PWM duty cycle
}

void Gpio::setPwmFreq(const uint32_t frequency) noexcept
{
    // Set PWM frequency
}

uint8_t Gpio::getGpioId() const noexcept
{
    return _gpio_id;
}

GpioState Gpio::getState() noexcept
{
    if (_direction == GpioDirection::kInput)
    {
        /* Read the current state from the GPIO controller */
        _state = _gpio_controller.getState(_gpio_id);
    }
    return _state;
}

GpioMode Gpio::getMode() const noexcept
{
    return _mode;
}

uint8_t Gpio::getPwmChannel() const noexcept
{
    return _pwm_channel;
}

uint8_t Gpio::getPwmTimer() const noexcept
{
    return _pwm_timer;
}

uint32_t Gpio::getPwmFrequency() const noexcept
{
    return _pwm_frequency;
}

uint8_t Gpio::getPwmDutyCycle() const noexcept
{
    return _pwm_duty_cycle;
}

} // namespace bsw