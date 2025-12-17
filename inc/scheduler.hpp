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
#include <vector>
#include "watchdog.hpp"

namespace bsw {

class Scheduler {
public:
    Scheduler(const uint32_t period_us, const uint32_t watchdog_timeout_ms = 100);
    ~Scheduler() = default;

    uint16_t init_timer();
    void tick_callback();

    bool add_task(const SchedulerTask& task);
    bool remove_task(const SchedulerTask& task);

    void start();
    void suspend();
    void resume();
private:
    static void tick_callback_wrapper(void* arg);

    uint32_t current_tick;
    uint32_t period_us;
    esp_timer_handle_t timer_handle;
    CoreTask scheduler_task;
    std::vector<bsw::SchedulerTask> scheduler_tasks;
    Watchdog watchdog;
};

} // namespace bsw