/**
 * @file scheduler.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>
#include <cstdio>
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

    bool start_on_core(uint8_t core_id, uint8_t uniqueId, uint8_t priority=5) noexcept;

    bool add_task(const SchedulerTask& task) noexcept;
    // bool remove_task(const SchedulerTask& task) noexcept;

    bool start() noexcept;
    void suspend() noexcept;
    void resume() noexcept;
private:
    static void tick_callback_wrapper_(void* arg) noexcept;
    bool start_worker_task_(const char* task_name, uint8_t priority, uint8_t core_id) noexcept;
    void signal_tick_() noexcept;
    void run_due_tasks_() noexcept;
    uint32_t consume_pending_ticks_() noexcept;

    void run_loop(); // The actual while(1) loop
    // Static bridge for FreeRTOS
    static void task_func_wrapper(void* param) {
        static_cast<Scheduler*>(param)->run_loop();
    }

    uint32_t current_tick_{0};
    uint32_t period_us_{kSchedulerPeriodUs}; // default 1 ms tick
    esp_timer_handle_t timer_handle_;
    CoreTask scheduler_core_task_;
    std::array<bsw::SchedulerTask, kMaxTasks> scheduler_tasks_;
    uint8_t task_count_ {0};
    volatile uint32_t pending_ticks_ {0};
    bool is_started_ {false};
    bool uses_periodic_task_ {false};
    Watchdog watchdog_;
    portMUX_TYPE state_lock_ = portMUX_INITIALIZER_UNLOCKED;
};

} // namespace bsw