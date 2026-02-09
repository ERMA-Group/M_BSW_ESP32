/**
 * @file scheduler.cpp
 * @brief C++ class implementation
 */

#include "scheduler.hpp"
#include <algorithm>

namespace bsw {
/* ---------------- C++ class ---------------- */
Scheduler::Scheduler(const uint32_t in_period_us, const uint32_t watchdog_timeout_ms)
    : current_tick_(0),
      period_us_(in_period_us),
      scheduler_tasks_()
{
    watchdog_.setTimeout(watchdog_timeout_ms);
    init_timer();
}

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

} // namespace bsw