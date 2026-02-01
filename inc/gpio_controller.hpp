/**
 * @file gpio_controller.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C façade.
 */

#pragma once
#include <cstdint>

namespace bsw {

enum class GpioDirection { kInput = 0, kOutput = 1 };
enum class GpioPullMode { kNone = 0, kPullUp = 1, kPullDown = 2, kPullUpDown = 3 };
enum class GpioState { kLow = 0, kHigh = 1 };
enum class GpioMode { kDigital = 0, kPwm = 1 };

constexpr uint8_t kPwmLookupTableSize = 101;

class GpioController {
public:
    explicit GpioController();
    ~GpioController() = default;

    void init();
    void setDirection(uint8_t gpio_id, GpioDirection direction);
    void setPullMode(uint8_t gpio_id, GpioPullMode pull_mode);
    void setGpioState(uint8_t gpio_id, GpioState state);
    GpioState getState(uint8_t gpio_id) const;
    void initPwm(uint8_t gpio_id, uint32_t frequency, uint8_t duty_cycle, uint8_t channel, uint8_t timer);
    void setPwmDuty(uint8_t channel, uint8_t duty_cycle, bool is_brightness=false, bool pwm_inverted=false);
    void setPwmFreq(uint8_t channel, uint32_t frequency);
    void stopPwm(uint8_t channel);

private:
    bool pwm_timer_initialized = false;
    uint16_t getPwmDutyCycleLookup(uint8_t duty_cycle) const;
    uint16_t getPwmBrightnessLookup(uint8_t brightness) const;
    uint16_t getPwmBrightnessLookupInv(uint8_t brightness) const;
    static const uint16_t pwm_lookup_table[kPwmLookupTableSize];
    static const uint16_t pwm_brightness_lookup_table[kPwmLookupTableSize];
    static const uint16_t pwm_brightness_lookup_table_inv[kPwmLookupTableSize];
};

} // namespace bsw