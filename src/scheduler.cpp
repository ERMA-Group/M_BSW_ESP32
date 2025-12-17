/**
 * @file scheduler.cpp
 * @brief C++ class implementation
 */

#include "scheduler.hpp"
#include <algorithm>

namespace bsw {
/* ---------------- C++ class ---------------- */
Scheduler::Scheduler(const uint32_t in_period_us, const uint32_t watchdog_timeout_ms)
    : current_tick(0),
      period_us(in_period_us),
      scheduler_tasks()
{
    watchdog.setTimeout(watchdog_timeout_ms);
    init_timer();
}

uint16_t Scheduler::init_timer()
{
    // 1. Define the timer arguments
    esp_timer_create_args_t timer_args = {
        .callback = &Scheduler::tick_callback_wrapper, // Set the static wrapper
        .arg = this,                                   // Pass the instance pointer (this)
        .dispatch_method = ESP_TIMER_TASK,             // Use the high-priority esp_timer task
        .name = "scheduler_timer"                      // A name for debugging
    };

    // 2. Create the timer
    esp_err_t err = esp_timer_create(&timer_args, &timer_handle);
    if (err != ESP_OK)
    {
        // failed to create timer
        return err;
    }

    return ESP_OK;
}

void Scheduler::tick_callback()
{
    current_tick++;
    watchdog.feed();
    // Iterate through all scheduled tasks
    for (auto& task : scheduler_tasks)
    {
        // Check if the task is due to run
        if (current_tick >= task.getTickWhenToRun())
        {
            // Execute the task's function
            task.execute();
            // Update the last run tick
            task.setLastSchedulerTick(current_tick);
        }
    }
}

bool Scheduler::add_task(const SchedulerTask& task)
{
    scheduler_tasks.push_back(task);
    return true;
}

bool Scheduler::remove_task(const SchedulerTask& task)
{
    auto it = std::find(scheduler_tasks.begin(), scheduler_tasks.end(), task);
    if (it != scheduler_tasks.end()) {
        scheduler_tasks.erase(it);
        return true;
    }
    return false;
}

void Scheduler::start()
{
    esp_timer_start_periodic(timer_handle, period_us);
}

void Scheduler::suspend()
{
    esp_timer_stop(timer_handle);
}

void Scheduler::resume()
{
    esp_timer_start_periodic(timer_handle, period_us);
}

// Static wrapper to call the member function
void Scheduler::tick_callback_wrapper(void* arg)
{
    // Cast the void* back to the class instance pointer
    Scheduler* self = static_cast<Scheduler*>(arg);
    self->tick_callback();
}

} // namespace bsw