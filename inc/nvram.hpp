/**
 * @file nvram.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include "nvs_flash.h"
#include "nvs.h"
#include <string>

extern "C" {

}

namespace bsw {

class Nvram {
private:
/**
 * @class Nvram
 * @brief Thin convenience wrapper around ESP-IDF NVS APIs.
 *
 * Responsibilities:
 * - Initialize NVS subsystem.
 * - Open/close a namespace handle.
 * - Read/write primitive values, strings, and blobs.
 * - Erase one key, one namespace, or full NVS partition.
 *
 * Usage notes:
 * - Call system_init() once before using read/write methods.
 * - Call open() before set/get/erase operations.
 * - close() is called automatically by the destructor.
 */
    nvs_handle_t _handle;
    std::string _namespace;
    /** @brief Underlying NVS handle for an opened namespace. */

    /** @brief Namespace name used when opening NVS. */
public:
    Nvram(const char* ns_name) : _handle(0), _namespace(ns_name) {}
    ~Nvram()
    /**
     * @brief Construct NVRAM wrapper for a given namespace.
     * @param ns_name NVS namespace name (max length defined by NVS).
     */
    {

    /**
     * @brief Destructor closes the handle if it is still open.
     */
        close();
    }
    static esp_err_t system_init()
    {

    /**
     * @brief Initialize NVS flash subsystem.
     *
     * If flash pages are full or version is incompatible, the partition
     * is erased and initialized again.
     *
     * @return ESP_OK on success, otherwise ESP-IDF error code.
     */
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            nvs_flash_erase();
            ret = nvs_flash_init();
        }
        return ret;
    }

    // Erase entire NVS partition and re-initialize it.
    static esp_err_t system_erase_all()
    /**
     * @brief Erase the whole default NVS partition and re-initialize it.
     * @return ESP_OK on success, otherwise ESP-IDF error code.
     */
    {
        esp_err_t ret = nvs_flash_erase();
        if (ret != ESP_OK)
        {
            return ret;
        }
        return nvs_flash_init();
    }

    esp_err_t open()
    /**
     * @brief Open this instance namespace in read/write mode.
     * @return ESP_OK on success, otherwise ESP-IDF error code.
     */
    {
        return nvs_open(_namespace.c_str(), NVS_READWRITE, &_handle);
    }

    void close()
    /**
     * @brief Close currently opened NVS handle.
     *
     * Safe to call multiple times.
     */
    {
        if (_handle != 0)
        {
            nvs_close(_handle);
            _handle = 0;
        }
    }

    // --- Template for Numbers and Booleans ---
    /**
     * @brief Store scalar value by key.
     *
     * Supported types: uint8_t, int32_t, uint32_t, bool.
     * Performs nvs_commit() on successful write.
     *
     * @tparam T Supported scalar type.
     * @param key NVS key.
     * @param value Value to store.
     * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED for unsupported type,
     *         or other ESP-IDF error code.
     */
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

    /**
     * @brief Read scalar value by key.
     *
     * Supported types: uint8_t, int32_t, uint32_t, bool.
     * If key is missing or read fails, default_val is returned.
     *
     * @tparam T Supported scalar type.
     * @param key NVS key.
     * @param default_val Value returned on failure/not found.
     * @return Loaded value or default_val.
     */
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

    /**
     * @brief Store zero-terminated string by key.
     *
     * Performs nvs_commit() on successful write.
     *
     * @param key NVS key.
     * @param value String value to store.
     * @return ESP_OK on success, otherwise ESP-IDF error code.
     */
    // --- Specialized String Handlers ---
    esp_err_t set_string(const char* key, const std::string& value)
    {
        esp_err_t err = nvs_set_str(_handle, key, value.c_str());
        if (err == ESP_OK) nvs_commit(_handle);
        return err;
    }
    /**
     * @brief Read string by key.
     *
     * Allocates temporary buffer based on required NVS size.
     * Returns empty string when key is missing or read fails.
     *
     * @param key NVS key.
     * @return Loaded string or empty string.
     */

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
    /**
     * @brief Store binary blob by key.
     *
     * Performs nvs_commit() on successful write.
     *
     * @param key NVS key.
     * @param data Pointer to raw bytes.
     * @param data_size Number of bytes to store.
     * @return ESP_OK on success, otherwise ESP-IDF error code.
     */

    // --- Binary Blob Handlers (non-breaking additive API) ---
    esp_err_t set_blob(const char* key, const void* data, size_t data_size)
    {
        esp_err_t err = nvs_set_blob(_handle, key, data, data_size);
        if (err == ESP_OK) nvs_commit(_handle);
        return err;
    /**
     * @brief Query blob size for a key.
     * @param key NVS key.
     * @param out_size Output size in bytes when key exists.
     * @return ESP_OK on success, otherwise ESP-IDF error code.
     */
    }

    esp_err_t get_blob_size(const char* key, size_t& out_size)
    {
        out_size = 0;
        return nvs_get_blob(_handle, key, nullptr, &out_size);
    /**
     * @brief Read a blob and validate exact expected size.
     *
     * First retrieves actual stored size, then compares it against
     * expected_size to prevent size mismatch reads.
     *
     * @param key NVS key.
     * @param data Destination buffer.
     * @param expected_size Exact number of bytes expected.
     * @return ESP_OK on success, ESP_ERR_NVS_INVALID_LENGTH on mismatch,
     *         or other ESP-IDF error code.
     */
    }

    esp_err_t get_blob(const char* key, void* data, size_t expected_size)
    {
        size_t required_size = 0;
        esp_err_t err = nvs_get_blob(_handle, key, nullptr, &required_size);
        if (err != ESP_OK)
        {
            return err;
        }
        if (required_size != expected_size)
        {
            return ESP_ERR_NVS_INVALID_LENGTH;
        }
        return nvs_get_blob(_handle, key, data, &required_size);
    /**
     * @brief Erase one key in currently opened namespace.
     *
     * Performs nvs_commit() after successful erase.
     *
     * @param key NVS key.
     * @return ESP_OK on success, otherwise ESP-IDF error code.
     */
    }

    esp_err_t erase_key(const char* key)
    {
        esp_err_t err = nvs_erase_key(_handle, key);
        if (err == ESP_OK) nvs_commit(_handle);
        return err;
    /**
     * @brief Erase all keys in currently opened namespace.
     *
     * Performs nvs_commit() after successful erase.
     *
     * @return ESP_OK on success, otherwise ESP-IDF error code.
     */
    }

    // Erase all keys inside the currently opened namespace.
    esp_err_t erase_all_in_namespace()
    {
        esp_err_t err = nvs_erase_all(_handle);
        if (err == ESP_OK) nvs_commit(_handle);
        return err;
    }
};

} // namespace bsw