/**
 * @file i2c.cpp
 * @brief C++ class implementation
 */

#include "i2c.hpp"   // C++ class definition

namespace bsw {
I2c::I2c(const Config& config) noexcept
    : config_(config) {
}

bool I2c::init() noexcept
{
    // 1. Configure the Bus
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = static_cast<i2c_port_t>(config_.module);
    bus_cfg.sda_io_num = (gpio_num_t)config_.sda_pin;
    bus_cfg.scl_io_num = (gpio_num_t)config_.scl_pin;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7; 

    if (i2c_new_master_bus(&bus_cfg, &_bus_handle) != ESP_OK) 
    {
        return false;
    }
    // 2. Configure the Device on that bus
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = (config_.addr_mode == AddrMode::kAddr7Bit) ? I2C_ADDR_BIT_LEN_7 : I2C_ADDR_BIT_LEN_10;
    dev_cfg.device_address = config_.device_addr;
    dev_cfg.scl_speed_hz = config_.clk_speed;
    dev_cfg.scl_wait_us = 0; // use default
    dev_cfg.flags.disable_ack_check = config_.ack_check_disable; // Disable ACK check. If this is set false, that means ack check is enabled, the transaction will be stopped and API returns error when nack is detected
    initialized_ = static_cast<bool>(i2c_master_bus_add_device(_bus_handle, &dev_cfg, &_dev_handle) == ESP_OK);
    return initialized_;
}

int32_t I2c::write_byte(const uint8_t reg_addr, const uint8_t data) noexcept
{
    uint8_t write_buf[2] = { reg_addr, data };
    // Synchronous write
    return static_cast<int32_t>(i2c_master_transmit(_dev_handle, write_buf, sizeof(write_buf), -1));
}

int32_t I2c::read_bytes(const uint8_t reg_addr, uint8_t* buffer, const size_t length) noexcept
{
    // Modern API handles the "Write Reg Addr -> Repeated Start -> Read Data" 
    // sequence in one efficient call
    return static_cast<int32_t>(i2c_master_transmit_receive(_dev_handle, &reg_addr, 1, buffer, length, -1));
}

} // namespace bsw