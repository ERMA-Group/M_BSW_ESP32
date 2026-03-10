/**
 * @file core_task.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

namespace bsw {

class CoreTask {
public:
    CoreTask();
    ~CoreTask() = default;

    bool create(void (*task_function)(void*), const char* name, uint16_t stack_size, void* parameters, uint8_t priority, uint8_t core) noexcept;
    TaskHandle_t getHandle() const noexcept { return task_handle; }
private:
    TaskHandle_t task_handle;
};

} // namespace bsw