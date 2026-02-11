/**
 * @file scheduler.cpp
 * @brief C++ class implementation
 */

#include "scheduler.hpp"
#include <algorithm>
#include "esp_task_wdt.h"

namespace bsw {
/* ---------------- C++ class ---------------- */
Scheduler::Scheduler(const uint32_t in_period_us, const uint32_t watchdog_timeout_ms)
    : current_tick_(0),
      period_us_(in_period_us),
      scheduler_tasks_()
{
    watchdog_.setTimeout(watchdog_timeout_ms);
}

/**
 * @brief Initializes the scheduler timer with the specified period and callback.
 * This function sets up an ESP timer that will call the scheduler's tick callback at regular intervals
 * defined by period_us_. The timer is configured to run in the context of the ESP timer task, which allows for precise timing and minimal jitter.
 * @return ESP_OK on success, or an error code if the timer could not be created
 */
uint16_t Scheduler::init_timer()
{
    // 1. Define the timer arguments
    esp_timer_create_args_t timer_args = {
        .callback = &Scheduler::tick_callback_wrapper_, // Set the static wrapper
        .arg = this,                                   // Pass the instance pointer (this)
        .dispatch_method = ESP_TIMER_TASK,             // Use the high-priority esp_timer task
        .name = "scheduler_timer"                      // A name for debugging
    };

    // 2. Create the timer
    esp_err_t err = esp_timer_create(&timer_args, &timer_handle_);
    if (err != ESP_OK)
    {
        // failed to create timer
        return err;
    }

    return ESP_OK;
}

void Scheduler::tick_callback()
{
    current_tick_++;
    watchdog_.feed();
    // Iterate through all scheduled tasks
    // ONLY iterate up to the number of tasks actually added
    for (uint8_t i = 0; i < task_count_; ++i) 
    {
        auto& task = scheduler_tasks_[i];

        if (current_tick_ >= task.getTickWhenToRun())
        {
            task.execute();
            task.setLastSchedulerTick(current_tick_);
        }
    }
}

bool Scheduler::add_task(const SchedulerTask& task)
{
    if (task_count_ >= kMaxTasks)
    {
        return false; // Array is full
    }

    scheduler_tasks_[task_count_] = task;
    task_count_++;
    return true;
}

// template <uint8_t MAXTASKS>
// bool Scheduler<MAXTASKS>::remove_task(const SchedulerTask& task)
// {
//     auto it = std::find(scheduler_tasks_.begin(), scheduler_tasks_.end(), task);
//     if (it != scheduler_tasks_.end()) {
//         scheduler_tasks_.erase(it);
//         return true;
//     }
//     return false;
// }

void Scheduler::start()
{
    esp_timer_start_periodic(timer_handle_, period_us_);
}

void Scheduler::suspend()
{
    esp_timer_stop(timer_handle_);
}

void Scheduler::resume()
{
    esp_timer_start_periodic(timer_handle_, period_us_);
}

// Static wrapper to call the member function
void Scheduler::tick_callback_wrapper_(void* arg)
{
    // Cast the void* back to the class instance pointer
    Scheduler* self = static_cast<Scheduler*>(arg);
    self->tick_callback();
}

/**
 * @brief Start the scheduler's main loop on a specific core with a given priority.
 * This function creates a FreeRTOS task that runs the scheduler's main loop, allowing it
 * to operate independently and with precise timing on the specified core.
 * @param core_id The ID of the core to run the scheduler on (0 or 1 for ESP32). Ensure that the core is not heavily used by other tasks to maintain scheduler performance.
 * @param uniqueId A unique identifier for the scheduler instance, used to create a unique task name. This is important if multiple schedulers are used to avoid task name conflicts.
 * @param priority The priority of the scheduler task (default is 5). Adjust this based on the needs of your application and the other tasks running on the system. Higher priority tasks will preempt lower priority ones, so choose wisely to ensure the scheduler runs smoothly without starving other critical tasks.
 * @note Minimum allowed period is 1ms - period_us_ 1000. If you need a faster tick use init_timer() and start() instead of this function to run the scheduler in the context of the ESP timer task, which allows for precise timing and minimal jitter.
 */
void Scheduler::start_on_core(uint8_t core_id, uint8_t uniqueId, uint8_t priority)
{
    // We don't need init_timer() if we run as a dedicated task
    // add uniqueid to task name to avoid conflicts if multiple schedulers are used
    char task_name[16];
    snprintf(task_name, sizeof(task_name), "SchedTask_%u", uniqueId);
    scheduler_core_task_.create(&Scheduler::task_func_wrapper, task_name, 8192, this, priority, core_id);
}

void Scheduler::run_loop()
{
    TickType_t last_wake = xTaskGetTickCount();
    // Calculate the delay in ticks for the desired period
    TickType_t frequency = pdMS_TO_TICKS(period_us_ / 1000);
    if (frequency == 0) {
        frequency = 1; 
    }

    while (true)
    {
        // 1. Run the existing tick logic
        this->tick_callback();

        // 2. Precise 1ms wait (accounting for execution time)
        vTaskDelayUntil(&last_wake, frequency);
        watchdog_.feed(); // Feed our own scheduler watchdog as well
    }
}

} // namespace bsw