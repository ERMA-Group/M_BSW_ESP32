/**
 * @file i2c.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>
#include "driver/i2c_master.h"

namespace bsw {

class I2c {
public:
    enum class AddrMode : uint8_t {
        kAddr7Bit = 0,
        kAddr10Bit = 1,
    };

    enum class AckType : uint8_t {
        kAck = 0x0,
        kNack = 0x1,
        kLastNack = 0x2,
    };

    enum class BusMode : uint8_t {
        kMaster = 0,
        kSlave = 1,
    };

    enum class SlaveReadWriteStatus : uint8_t {
        kWriteByMaster = 0,
        kReadByMaster = 1,
    };

    enum class Module : uint8_t {
        kI2c0 = I2C_NUM_0,
#if SOC_HP_I2C_NUM >= 2
        kI2c1 = I2C_NUM_1,
#endif /* SOC_HP_I2C_NUM >= 2 */
#if SOC_LP_I2C_NUM >= 1
        kLpI2c0 = LP_I2C_NUM_0,
#endif /* SOC_LP_I2C_NUM >= 1 */
    };

    struct Config {
        Module module;        // I2C_NUM_0 or I2C_NUM_1
        uint8_t sda_pin;
        uint8_t scl_pin;
        uint16_t device_addr;   // 7/10-bit slave address
        AddrMode addr_mode;     // 7-bit or 10-bit address mode
        uint32_t clk_speed;     // e.g., 400000 (400kHz)
        bool ack_check_disable;   // Disable ACK check. If this is set false, that means ack check is enabled, the transaction will be stopped and API returns error when nack is detected
    };

    I2c() noexcept = default;
    explicit I2c(const Config& config) noexcept;
    ~I2c() noexcept = default;

    bool init() noexcept;

    int32_t write_byte(const uint8_t reg_addr, const uint8_t data) noexcept;
    int32_t read_bytes(const uint8_t reg_addr, uint8_t* buffer, const size_t length) noexcept;

private:
    bool initialized_ = false;
    Config config_;
    i2c_master_bus_handle_t _bus_handle;
    i2c_master_dev_handle_t _dev_handle;
};

} // namespace bsw