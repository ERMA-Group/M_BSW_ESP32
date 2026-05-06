/**
 * @file nvram_storage.hpp
 * @brief Reusable typed NVRAM store helper built on bsw::Nvram.
 *
 * Design goals:
 * - Keep NVRAM persistence strongly typed in C++.
 * - Validate payload compatibility on load (magic, version, payload size).
 * - Keep project-level code small: save/load/erase one typed payload.
 *
 * Scope model:
 * - One NvramStorage<TPayload> instance manages exactly one NVS entry
 *   addressed by one namespace + one key.
 * - The payload can represent either one small settings group or a full
 *   application settings snapshot, depending on project needs.
 * - It does NOT own the whole NVS partition; other modules can store data in
 *   other namespaces/keys independently.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "nvram.hpp"

namespace bsw {

/**
 * @brief Header stored in front of each typed NVRAM payload.
 *
 * The header allows the loader to reject incompatible data before the
 * payload is copied back into application state. The combination of magic,
 * version, and payload size is used as a lightweight integrity and
 * compatibility check.
 */
struct NvramTypedHeader {
    /// Application-specific identifier for the stored record format.
    uint32_t magic;
    /// Schema version expected by the current firmware.
    uint16_t version;
    /// Serialized payload size in bytes.
    uint16_t payloadSize;
};

/**
 * @brief On-flash representation of a typed NVRAM entry.
 *
 * The entry is written as one blob so the header and payload are persisted
 * atomically from the caller's perspective. The header stays small and fixed
 * so that a load operation can validate the blob before exposing the payload
 * to the caller.
 *
 * @tparam TPayload Payload type stored after the header.
 */
template <typename TPayload>
struct NvramTypedEntry {
    /// Metadata used to validate the entry before loading the payload.
    NvramTypedHeader header;
    /// Persisted payload bytes, copied verbatim to and from NVRAM.
    TPayload payload;
};

/**
 * @brief Small helper for saving, loading, and erasing one typed NVRAM entry.
 *
 * The class keeps the public API intentionally narrow: one namespace, one
 * key, one payload type, and one validation policy. It does not attempt to
 * manage NVS namespaces or own the underlying partition. Instead, it focuses
 * on a single typed record that can be saved, reloaded, or removed with a
 * consistent schema check.
 *
 * @tparam TPayload Trivially copyable payload type to store.
 */
template <typename TPayload>
class NvramStorage {
    static_assert(std::is_trivially_copyable_v<TPayload>, "TPayload must be trivially copyable.");

public:
    /**
     * @brief Creates a storage helper for one namespace/key pair.
     *
        * The constructor only records the identifiers and the compatibility
        * values. No NVS access happens here, which keeps object construction
        * cheap and allows the helper to be created at configuration time.
        *
     * @param nameSpace NVS namespace.
     * @param key NVS key.
     * @param magic Expected magic value for the payload.
     * @param version Expected payload version.
     */
    NvramStorage(const char* nameSpace, const char* key, uint32_t magic, uint16_t version) noexcept
        : ns_(nameSpace), key_(key), magic_(magic), version_(version)
    {
    }

    /**
     * @brief Saves a payload to NVRAM.
     *
        * The payload is wrapped in a small typed envelope before being written
        * as a blob. A save fails if NVS initialization fails, the namespace
        * cannot be opened, or the blob write returns an error. The method closes
        * the NVS handle before returning in both the success and failure paths.
        *
     * @param payload Payload to persist.
     * @return true on success, false on any NVRAM or serialization failure.
     */
    bool save(const TPayload& payload) const
    {
        if (Nvram::system_init() != ESP_OK)
        {
            return false;
        }

        Nvram nvs { ns_ };
        if (nvs.open() != ESP_OK)
        {
            return false;
        }

        NvramTypedEntry<TPayload> entry {};
        entry.header.magic = magic_;
        entry.header.version = version_;
        entry.header.payloadSize = static_cast<uint16_t>(sizeof(TPayload));
        entry.payload = payload;

        const esp_err_t err = nvs.set_blob(key_, &entry, sizeof(entry));
        nvs.close();
        return err == ESP_OK;
    }

    /**
     * @brief Loads a payload from NVRAM.
     *
        * The method reads the full stored blob into a temporary typed entry and
        * then verifies that the magic value, schema version, and payload size
        * match the expectations captured by this instance. If any check fails,
        * the output payload is left unchanged and the method reports failure.
        *
     * @param[out] outPayload Receives the stored payload on success.
     * @return true if a compatible entry was found and decoded.
     */
    bool load(TPayload& outPayload) const
    {
        if (Nvram::system_init() != ESP_OK)
        {
            return false;
        }

        Nvram nvs { ns_ };
        if (nvs.open() != ESP_OK)
        {
            return false;
        }

        NvramTypedEntry<TPayload> entry {};
        const esp_err_t err = nvs.get_blob(key_, &entry, sizeof(entry));
        nvs.close();
        if (err != ESP_OK)
        {
            return false;
        }

        if (entry.header.magic != magic_ ||
            entry.header.version != version_ ||
            entry.header.payloadSize != sizeof(TPayload))
        {
            return false;
        }

        outPayload = entry.payload;
        return true;
    }

    /**
     * @brief Erases the stored entry.
     *
        * Missing keys are treated as a non-error so callers can use erase as a
        * cleanup step without having to distinguish between "already gone" and
        * "successfully removed".
        *
     * @return true if the key was removed or was already missing.
     */
    bool erase() const
    {
        if (Nvram::system_init() != ESP_OK)
        {
            return false;
        }

        Nvram nvs { ns_ };
        if (nvs.open() != ESP_OK)
        {
            return false;
        }

        const esp_err_t err = nvs.erase_key(key_);
        nvs.close();
        return (err == ESP_OK) || (err == ESP_ERR_NVS_NOT_FOUND);
    }

private:
    /// Namespace that groups the stored record in NVS.
    const char* ns_;
    /// Key used for the typed entry within the namespace.
    const char* key_;
    /// Expected magic value for validating loaded data.
    uint32_t magic_;
    /// Expected schema version for validating loaded data.
    uint16_t version_;
};

} // namespace bsw
