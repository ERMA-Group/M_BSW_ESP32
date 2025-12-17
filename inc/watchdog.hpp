/**
 * @file watchdog.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>

namespace bsw {

class Watchdog {
public:
    /* The value that needs to be written to TIMG_WDT_WKEY to write-enable the wdt registers */
    static constexpr uint32_t kTimgWdtWkeyValue = 0x50D83AA1;

    Watchdog() noexcept = default;
    Watchdog(uint32_t timeout_ms) noexcept : timeout_ms(timeout_ms) {}
    ~Watchdog() noexcept = default;

    /** Feed the watchdog if the timeout has elapsed */
    void feed() noexcept;

    /** Feed the watchdog immediately, bypassing the timeout check */
    void directFeed() noexcept;

    void setTimeout(uint32_t timeout) noexcept { timeout_ms = timeout; }
private:
    uint64_t last_feed_time_us = 0;
    uint32_t timeout_ms = 0;
};

} // namespace bsw