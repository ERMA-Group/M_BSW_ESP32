/**
 * @file spi.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>
#include <cstring> // memcpy
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
        // uint8_t command_bits; // 0-16
        // uint8_t address_bits; // 0-64
        // uint8_t dummy_bits;
    };

    Spi() noexcept = default;
    explicit Spi(const Config& config) noexcept;
    ~Spi() noexcept = default;

    void configure(const Config& config) noexcept;
    bool init() noexcept;
    bool isInitialized() const noexcept { return initialized_; }

    void transfer(const uint8_t reg, const uint8_t* tx_data, uint8_t* rx_data, const uint16_t len) noexcept;
    void write_burst(const uint8_t reg, const uint8_t* data, const uint16_t len) noexcept;
    void write_burst16(const uint16_t reg, const uint8_t* data, const uint16_t len) noexcept;
    void read_burst(const uint8_t reg, uint8_t* buffer, const uint16_t len) noexcept;
    void read_burst16(const uint16_t reg, uint8_t* buffer, const uint16_t len) noexcept;
    uint8_t read_byte(const uint8_t reg) noexcept;
    uint8_t read_byte16(const uint16_t reg) noexcept;
    void write_byte(const uint8_t reg, const uint8_t data) noexcept;
    void write_byte16(const uint16_t reg, const uint8_t data) noexcept;


private:
    bool initialized_ = false;
    Config config_{};
    spi_device_handle_t device_handle_ = nullptr;
};

}