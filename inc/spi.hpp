/**
 * @file spi.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>
#include "driver/spi_master.h"
#include "driver/gpio.h"

namespace bsw {

class Spi {
public:
    static constexpr uint16_t kDefaultQueueSize = 7;

    enum class Mode : uint8_t {
        kMode0 = 0, // CPOL = 0, CPHA = 0
        kMode1 = 1, // CPOL = 0, CPHA = 1
        kMode2 = 2, // CPOL = 1, CPHA = 0
        kMode3 = 3  // CPOL = 1, CPHA = 1
    };

    enum class Host : uint8_t {
        kSpi1 = SPI1_HOST,
        kSpi2 = SPI2_HOST,
        kSpi3 = SPI3_HOST,
    };

    struct Config {
        Host host;
        Mode mode;
        uint8_t mosi_pin;
        uint8_t miso_pin;
        uint8_t sck_pin;
        int8_t cs_pin; // -1 if not used
        uint32_t clock_speed_hz;
        uint16_t queue_size; // Number of transactions that can be queued; 0 for default 
        uint8_t dma_chan; // 0 for no DMA
        uint8_t command_bits; // 0-16
        uint8_t address_bits; // 0-64
        uint8_t dummy_bits;
    };

    Spi() noexcept = default;
    explicit Spi(const Config& config) noexcept;
    ~Spi() noexcept = default;

    bool init() noexcept;

    int32_t transfer(const uint8_t* tx_data, uint8_t* rx_data, const uint16_t length) noexcept;
    int32_t write_cmd(const uint8_t cmd) noexcept;

private:
    bool initialized_ = false;
    Config config_{};
    spi_device_handle_t device_handle_ = nullptr;
};

}