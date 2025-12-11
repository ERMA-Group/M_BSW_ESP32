/**
 * @file scheduler_task.cpp
 * @brief C++ class implementation
 */

#include "scheduler_task.hpp"

namespace bsw {
/* ---------------- C++ class ---------------- */
SchedulerTask::SchedulerTask(void (*task_function)(void*), uint8_t priority, uint16_t run_every_scheduler_ticks)
    : scheduler_ticks(run_every_scheduler_ticks),
      priority(priority),
      last_scheduler_tick(0)
{}

void SchedulerTask::execute()
{
    if (task_function != nullptr)
    {
        task_function(nullptr);
    }
    else
    {
        // Handle null function pointer if necessary
    }
}

uint16_t SchedulerTask::getSchedulerTicks() const
{
    return scheduler_ticks;
}

uint32_t SchedulerTask::getLastSchedulerTick() const
{
    return last_scheduler_tick;
}
uint8_t SchedulerTask::getPriority() const
{
    return priority;
}
uint32_t SchedulerTask::getTickWhenToRun() const
{
    return static_cast<uint32_t>(last_scheduler_tick + scheduler_ticks);
}
void SchedulerTask::setLastSchedulerTick(uint32_t tick)
{
    last_scheduler_tick = tick;
}
void SchedulerTask::setPriority(uint8_t prio)
{
    priority = prio;
}
void SchedulerTask::setSchedulerTicks(uint16_t ticks)
{
    scheduler_ticks = ticks;
}

} // namespace bsw