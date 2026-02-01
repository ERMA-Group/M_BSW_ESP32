/**
 * @file nvram.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>
#include "nvs_flash.h"
#include "nvs.h"
#include <string>

extern "C" {

}

namespace bsw {

class Nvram {
private:
    nvs_handle_t _handle;
    std::string _namespace;

public:
    Nvram(const char* ns_name) : _handle(0), _namespace(ns_name) {}
    ~Nvram()
    {
        close();
    }
    static esp_err_t system_init()
    {
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            nvs_flash_erase();
            ret = nvs_flash_init();
        }
        return ret;
    }

    esp_err_t open()
    {
        return nvs_open(_namespace.c_str(), NVS_READWRITE, &_handle);
    }

    void close()
    {
        if (_handle != 0) nvs_close(_handle);
    }

    // --- Template for Numbers and Booleans ---
    template <typename T>
    esp_err_t set_value(const char* key, T value)
    {
        esp_err_t err;
        if constexpr (std::is_same_v<T, uint8_t>)  err = nvs_set_u8(_handle, key, value);
        else if constexpr (std::is_same_v<T, int32_t>) err = nvs_set_i32(_handle, key, value);
        else if constexpr (std::is_same_v<T, uint32_t>) err = nvs_set_u32(_handle, key, value);
        else if constexpr (std::is_same_v<T, bool>)     err = nvs_set_u8(_handle, key, value ? 1 : 0);
        else return ESP_ERR_NOT_SUPPORTED;

        if (err == ESP_OK) nvs_commit(_handle);
        return err;
    }

    template <typename T>
    T get_value(const char* key, T default_val)
    {
        T value = default_val;
        if constexpr (std::is_same_v<T, uint8_t>)  nvs_get_u8(_handle, key, &value);
        else if constexpr (std::is_same_v<T, int32_t>) nvs_get_i32(_handle, key, &value);
        else if constexpr (std::is_same_v<T, uint32_t>) nvs_get_u32(_handle, key, &value);
        else if constexpr (std::is_same_v<T, bool>) {
            uint8_t temp;
            if (nvs_get_u8(_handle, key, &temp) == ESP_OK) value = (temp == 1);
        }
        return value;
    }

    // --- Specialized String Handlers ---
    esp_err_t set_string(const char* key, const std::string& value)
    {
        esp_err_t err = nvs_set_str(_handle, key, value.c_str());
        if (err == ESP_OK) nvs_commit(_handle);
        return err;
    }

    std::string get_string(const char* key)
    {
        size_t required_size;
        if (nvs_get_str(_handle, key, nullptr, &required_size) != ESP_OK) return "";
        char* buf = new char[required_size];
        nvs_get_str(_handle, key, buf, &required_size);
        std::string res(buf);
        delete[] buf;
        return res;
    }
};

} // namespace bsw