/**
 * @file time.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"
#include "esp_timer.h" // Required for high-res timer

namespace bsw {

class Time {
public:
    Time() = default;
    ~Time() = default;

    /**
     * @brief Initialize SNTP and set timezone
     * @param sntp_server The SNTP server to use (default: "pool.ntp.org")
     */
    void init(const char* sntp_server = "pool.ntp.org") {
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, sntp_server);
        sntp_init();

        setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
        tzset();
    }

    // Helper to check if SNTP actually worked
    bool isSynced() const noexcept {
        return (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED);
    }

    /**
     * @brief Wall Clock: Seconds since midnight (Local Time)
     */
    int32_t getSecondsSinceMidnight() const noexcept{
        if (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) return -1;

        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        return (timeinfo.tm_hour * 3600) + (timeinfo.tm_min * 60) + timeinfo.tm_sec;
    }

    /**
     * @brief Uptime: Absolute time in microseconds since boot
     */
    int64_t getUptimeUs() const noexcept {
        return esp_timer_get_time();
    }

    /**
     * @brief Uptime: Milliseconds since boot
     */
    uint32_t getUptimeMs() const noexcept {
        return (uint32_t)(esp_timer_get_time() / 1000);
    }

    /**
     * @brief Returns raw Unix Timestamp (Seconds since 1970)
     */
    uint32_t getUnixTimestamp() const noexcept {
        time_t now;
        time(&now);
        return (uint32_t)now;
    }
};

} // namespace bsw