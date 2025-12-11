/**
 * @file core_task.cpp
 * @brief C++ class implementation
 */

#include "core_task.hpp"

namespace bsw {
/* ---------------- C++ class ---------------- */
CoreTask::CoreTask() : task_handle(nullptr) {}
void CoreTask::create(void (*task_function)(void*), const char* name, uint16_t stack_size, void* parameters, uint8_t priority, uint8_t core)
{
    xTaskCreatePinnedToCore(
        task_function,
        name,
        stack_size,
        parameters,
        priority,
        &task_handle,
        core
    );
}

} // namespace bsw