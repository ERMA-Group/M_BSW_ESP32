/**
 * @file spi.cpp
 * @brief C++ class implementation
 */

#include "spi.hpp"   // C++ class definition

namespace bsw {
Spi::Spi(const Config& config) noexcept
    : config_(config) {
}

bool Spi::init() noexcept {
    if (initialized_)
    {
        return true;
    }

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = config_.mosi_pin;
    buscfg.miso_io_num = config_.miso_pin;
    buscfg.sclk_io_num = config_.sck_pin;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 4096; // Default max transfer size

    esp_err_t ret = spi_bus_initialize(static_cast<spi_host_device_t>(config_.host), &buscfg, config_.dma_chan);
    if (ret != ESP_OK)
    {
        return false;
    }

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = config_.clock_speed_hz;
    devcfg.mode = static_cast<uint8_t>(config_.mode);
    devcfg.spics_io_num = config_.cs_pin;
    if (config_.queue_size != 0)
    {
        devcfg.queue_size = config_.queue_size;
    }
    else
    {
        devcfg.queue_size = kDefaultQueueSize;
    }

    /* prepare device handler */
    device_handle_ = nullptr;
    ret = spi_bus_add_device(static_cast<spi_host_device_t>(config_.host), &devcfg, &device_handle_);
    if (ret != ESP_OK)
    {
        return false;
    }
    initialized_ = true;
    return true;
}

int32_t Spi::transfer(const uint8_t* tx_data, uint8_t* rx_data, const uint16_t length) noexcept 
{
    spi_transaction_t t = {};
    t.length = length * 8;      // Length is in bits
    t.tx_buffer = tx_data;
    t.rx_buffer = rx_data;

    return static_cast<int32_t>(spi_device_transmit(device_handle_, &t));
}

int32_t Spi::write_cmd(uint8_t cmd) noexcept
{
    spi_transaction_t t = {};
    t.length = 8;                      // Total 8 bits
    t.flags = SPI_TRANS_USE_TXDATA;    // Tell driver to use internal buffer
    t.tx_data[0] = cmd;                // Load the byte directly into the struct
    
    return static_cast<int32_t>(spi_device_transmit(device_handle_, &t));
}


} // namespace bsw