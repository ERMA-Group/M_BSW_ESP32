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
    Gpio(GpioController& gpio_controller, const uint8_t gpio_id, const GpioDirection direction, const GpioPullMode pull_mode, const GpioState initial_state) noexcept;
    Gpio(
        GpioController& gpio_controller, 
        const uint8_t gpio_id, 
        const GpioDirection direction, 
        const GpioPullMode pull_mode, 
        const GpioState initial_state, 
        const uint8_t pwm_channel, 
        const uint8_t pwm_timer,
        const uint32_t pwm_frequency, 
        const uint8_t pwm_duty_cycle
    ) noexcept;
    ~Gpio() = default;

    void init() noexcept;

    void initPwm(const uint32_t frequency, const uint8_t duty_cycle, const uint8_t channel, const uint8_t timer) noexcept;
    void setDirection(const GpioDirection direction) noexcept;
    void setPullMode(const GpioPullMode pull_mode) noexcept;
    void setState(const GpioState state) noexcept;
    void toggleGpioState() noexcept;

    void setGpioId(const uint8_t gpio_id) noexcept;
    void setPwmDuty(const uint8_t duty_cycle, bool is_brightness=false, bool pwm_inverted=false) noexcept;
    void setPwmFreq(const uint32_t frequency) noexcept;
    uint8_t getGpioId() const noexcept;
    GpioState getState() noexcept;
    GpioMode getMode() const noexcept;
    uint8_t getPwmChannel() const noexcept;
    uint8_t getPwmTimer() const noexcept;
    uint32_t getPwmFrequency() const noexcept;
    uint8_t getPwmDutyCycle() const noexcept;
private:
    GpioController& _gpio_controller;
    uint8_t _gpio_id;
    GpioDirection _direction;
    GpioPullMode _pull_mode;
    GpioState _state;
    GpioMode _mode;
    uint8_t _pwm_channel;
    uint8_t _pwm_timer;
    uint32_t _pwm_frequency;
    uint8_t _pwm_duty_cycle;
};

} // namespace bsw