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
    using TaskFunc = void (*)(void*); // Typedef for clarity

public:
    SchedulerTask() noexcept = default;
    SchedulerTask(const TaskFunc func, const uint8_t priority, const uint16_t run_every_scheduler_ticks, void* arg = nullptr) noexcept;
    ~SchedulerTask() noexcept = default;

    void construct(const TaskFunc func, const uint8_t priority, const uint16_t run_every_scheduler_ticks, void* arg = nullptr) noexcept;

    void execute() noexcept;

    uint16_t getSchedulerTicks() const noexcept;
    uint32_t getLastSchedulerTick() const noexcept;
    uint8_t getPriority() const noexcept;
    uint32_t getTickWhenToRun() const noexcept;

    void setTaskFunction(TaskFunc func) noexcept;
    void setLastSchedulerTick(uint32_t tick) noexcept;
    void setPriority(uint8_t prio) noexcept;
    void setSchedulerTicks(uint16_t ticks) noexcept;

    bool operator==(const SchedulerTask& other) const
    {
        return _task_function == other._task_function;
    }

private:
    TaskFunc _task_function {nullptr}; // Function pointer for the task
    uint16_t _scheduler_ticks {0}; // How often the task should run in scheduler ticks
    uint8_t _priority {0}; // Task priority
    uint32_t _last_scheduler_tick {0}; // Last tick when the task was run
    void* _arg {nullptr}; // Argument to pass to the task function
};

} // namespace bsw