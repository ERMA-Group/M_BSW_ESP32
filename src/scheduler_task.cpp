/**
 * @file scheduler_task.cpp
 * @brief C++ class implementation
 */

#include "scheduler_task.hpp"

namespace bsw {
/* ---------------- C++ class ---------------- */
SchedulerTask::SchedulerTask(const TaskFunc func, const uint8_t priority, const uint16_t run_every_scheduler_ticks) noexcept
    : _task_function(func),
      _scheduler_ticks(run_every_scheduler_ticks),
      _priority(priority),
      _last_scheduler_tick(0)
{}

void SchedulerTask::construct(const TaskFunc func, const uint8_t priority, const uint16_t run_every_scheduler_ticks) noexcept
{
    _task_function = func;
    _priority = priority;
    _scheduler_ticks = run_every_scheduler_ticks;
    _last_scheduler_tick = 0;
}

void SchedulerTask::execute() noexcept
{
    if (_task_function != nullptr)
    {
        _task_function(nullptr);
    }
    else
    {
        // Handle null function pointer if necessary
    }
}

uint16_t SchedulerTask::getSchedulerTicks() const noexcept
{
    return _scheduler_ticks;
}

uint32_t SchedulerTask::getLastSchedulerTick() const noexcept
{
    return _last_scheduler_tick;
}
uint8_t SchedulerTask::getPriority() const noexcept
{
    return _priority;
}
uint32_t SchedulerTask::getTickWhenToRun() const noexcept
{
    return static_cast<uint32_t>(_last_scheduler_tick + _scheduler_ticks);
}
void SchedulerTask::setTaskFunction(TaskFunc func) noexcept
{
    _task_function = func;
}
void SchedulerTask::setLastSchedulerTick(uint32_t tick) noexcept
{
    _last_scheduler_tick = tick;
}
void SchedulerTask::setPriority(uint8_t prio) noexcept
{
    _priority = prio;
}
void SchedulerTask::setSchedulerTicks(uint16_t ticks) noexcept
{
    _scheduler_ticks = ticks;
}

} // namespace bsw