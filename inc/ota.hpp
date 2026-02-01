/**
 * @file ota.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"

namespace bsw {

class Ota {
public:
    esp_err_t start_update(const char* url);

    void cancel_rollback();
    
private:
};

} // namespace bsw