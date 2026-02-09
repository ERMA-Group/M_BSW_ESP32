/**
 * @file scheduler.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>
#include "esp_timer.h"
#include "core_task.hpp"
#include "scheduler_task.hpp"
#include <array>
#include "watchdog.hpp"

namespace bsw {

class Scheduler {
public:
    static constexpr uint16_t kMaxTasks {32}; // default 1 ms tick
    static constexpr uint16_t kSchedulerPeriodUs {1000}; // default 1 ms tick

    Scheduler() noexcept = default;
    Scheduler(const uint32_t period_us, const uint32_t watchdog_timeout_ms = 100) noexcept;
    ~Scheduler() noexcept = default;

    void setPeriod(uint32_t period_us) noexcept { period_us_ = period_us; }
    void setWatchdogTimeout(uint32_t timeout_ms) noexcept { watchdog_.setTimeout(timeout_ms); }

    uint16_t init_timer() noexcept;
    void tick_callback() noexcept;

    bool add_task(const SchedulerTask& task) noexcept;
    // bool remove_task(const SchedulerTask& task) noexcept;

    void start() noexcept;
    void suspend() noexcept;
    void resume() noexcept;
private:
    static void tick_callback_wrapper_(void* arg) noexcept;

    uint32_t current_tick_{0};
    uint32_t period_us_{kSchedulerPeriodUs}; // default 1 ms tick
    esp_timer_handle_t timer_handle_;
    CoreTask scheduler_core_task_;
    std::array<bsw::SchedulerTask, kMaxTasks> scheduler_tasks_;
    uint8_t task_count_ {0};
    Watchdog watchdog_;
};

} // namespace bsw