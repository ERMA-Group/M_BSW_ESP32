/**
 * @file scheduler_task.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once

#include <cstdint>

namespace bsw {

class SchedulerTask {
public:
    SchedulerTask(void (*task_function)(void*), uint8_t priority, uint16_t run_every_scheduler_ticks);
    ~SchedulerTask() = default;

    void execute();

    uint16_t getSchedulerTicks() const;
    uint32_t getLastSchedulerTick() const;
    uint8_t getPriority() const;
    uint32_t getTickWhenToRun() const;

    void setLastSchedulerTick(uint32_t tick);
    void setPriority(uint8_t prio);
    void setSchedulerTicks(uint16_t ticks);

private:
    void (*task_function)(void*);
    uint16_t scheduler_ticks; // How often the task should run in scheduler ticks
    uint8_t priority; // Task priority
    uint32_t last_scheduler_tick; // Last tick when the task was run
};

} // namespace bsw