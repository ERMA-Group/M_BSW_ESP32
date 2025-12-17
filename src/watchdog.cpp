/**
 * @file watchdog.cpp
 * @brief C++ class implementation
 */

#include "watchdog.hpp"   // C++ class definition
// #include "driver/timer.h"
// #include <rtc_wdt.h>
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "esp_system.h"
#include "esp_timer.h"

namespace bsw {
/* ---------------- C++ class ---------------- */

void Watchdog::feed() noexcept
{
    uint64_t current_time_us = esp_timer_get_time();
    if (current_time_us > last_feed_time_us + (static_cast<uint64_t>(timeout_ms) * 1000ULL))
    {
        last_feed_time_us = current_time_us;
        directFeed();
    }
    else
    {
        /** Skip feeding the watchdog as the timeout has not yet elapsed */
    }
}

void Watchdog::directFeed() noexcept
{
    // Unlock WDT write protection
    TIMERG0.wdtwprotect.val = kTimgWdtWkeyValue;
    // Feed the watchdog
    TIMERG0.wdtfeed.val = 1;
    // Re-lock WDT write protection
    TIMERG0.wdtwprotect.val = 0;
}

} // namespace bsw