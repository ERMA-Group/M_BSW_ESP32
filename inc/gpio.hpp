/**
 * @file gpio.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>
#include "gpio_controller.hpp"

namespace bsw {

class Gpio {
public:
    Gpio(GpioController& gpio_controller, uint8_t gpio_id, GpioDirection direction, GpioPullMode pull_mode, GpioState initial_state);
    ~Gpio() = default;

    void initPwm(uint8_t gpio_id, uint32_t frequency, uint8_t duty_cycle, uint8_t channel, uint8_t timer);
    void setDirection(uint8_t gpio_id, GpioDirection direction);
    void setPullMode(uint8_t gpio_id, GpioPullMode pull_mode);
    void setGpioState(uint8_t gpio_id, GpioState state);

    void setPwmDuty(uint8_t gpio_id, uint8_t duty_cycle);
    void setPwmFreq(uint8_t gpio_id, uint32_t frequency);
    GpioState getState(uint8_t gpio_id) const;
    GpioMode getMode() const;
    uint8_t getPwmChannel() const;
    uint8_t getPwmTimer() const;
    uint32_t getPwmFrequency() const;
    uint8_t getPwmDutyCycle() const;
private:
    GpioController& gpio_controller;
    uint8_t gpio_id;
    GpioDirection direction;
    GpioPullMode pull_mode;
    GpioState state;
    GpioMode mode;
    uint8_t pwm_channel;
    uint8_t pwm_timer;
    uint32_t pwm_frequency;
    uint8_t pwm_duty_cycle;
};

} // namespace bsw