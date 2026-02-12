/**
 * @file spi.cpp
 * @brief C++ class implementation
 */

#include "spi.hpp"   // C++ class definition

namespace bsw {
Spi::Spi(const Config& config) noexcept
    : config_(config) {
}

void Spi::configure(const Config& config) noexcept
{
    config_ = config;
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

// --- 8-BIT ADDRESS METHODS ---

void Spi::transfer(const uint8_t reg, const uint8_t* tx_data, uint8_t* rx_data, const uint16_t len) noexcept
{
    if (!isInitialized()) return;

    // 1. Prepare the combined buffer (Address + Data)
    // We use a small local buffer for performance; larger transfers use heap.
    const size_t total_len = len + 1;
    uint8_t* local_tx = nullptr;
    uint8_t* local_rx = nullptr;

    if (total_len <= 64) {
        local_tx = (uint8_t*)alloca(total_len); // Fast stack allocation
        local_rx = (uint8_t*)alloca(total_len);
    } else {
        local_tx = (uint8_t*)heap_caps_malloc(total_len, MALLOC_CAP_DMA);
        local_rx = (uint8_t*)heap_caps_malloc(total_len, MALLOC_CAP_DMA);
    }

    if (!local_tx || !local_rx) return;

    // 2. Setup TX data: Address first, then payload
    local_tx[0] = reg;
    if (tx_data && len > 0) {
        memcpy(&local_tx[1], tx_data, len);
    } else {
        // If no TX data provided, fill with dummy 0x00 to clock out RX
        memset(&local_tx[1], 0x00, len);
    }

    // 3. Define the Transaction
    spi_transaction_t t = {};
    t.length = total_len * 8; // Total bits
    t.tx_buffer = local_tx;
    t.rx_buffer = local_rx;

    // 4. Execute (Polling for speed if small, Interrupt for large)
    if (total_len <= 32) {
        spi_device_polling_transmit(device_handle_, &t);
    } else {
        spi_device_transmit(device_handle_, &t);
    }

    // 5. Copy received data back to user buffer (skipping the first byte junk)
    if (rx_data && len > 0) {
        memcpy(rx_data, &local_rx[1], len);
    }

    // 6. Cleanup if we used the heap
    if (total_len > 64) {
        free(local_tx);
        free(local_rx);
    }
}

uint8_t Spi::read_byte(const uint8_t reg) noexcept 
{
    if (!isInitialized()) return 0x00;

    // Use a 2-byte array for the command and dummy clock
    // reg is already handled externally for the Read/Write bit
    uint8_t tx_data[2] = { reg, 0x00 };

    spi_transaction_t t = {};
    t.length = 16;                                // Total bits (8 addr + 8 data)
    t.tx_buffer = tx_data;
    t.flags = SPI_TRANS_USE_RXDATA;              // Store result in t.rx_data

    // Using polling for the lowest possible latency inside your 1ms tick
    esp_err_t ret = spi_device_polling_transmit(device_handle_, &t);
    
    if (ret != ESP_OK) {
        return 0x00;
    }

    // t.rx_data[0] is the junk byte from the address phase
    // t.rx_data[1] is the actual register value
    return t.rx_data[1];
}

void Spi::write_byte(const uint8_t reg, const uint8_t data) noexcept {
    if (!isInitialized()) return;

    uint8_t tx_buf[2] = { reg, data };

    spi_transaction_t t = {};
    t.length = 16; // 16 bits total (8 addr + 8 data)
    t.tx_buffer = tx_buf;

    spi_device_transmit(device_handle_, &t);
}

void Spi::write_burst(const uint8_t reg, const uint8_t* data, const uint16_t len) noexcept 
{
    if (!isInitialized() || len == 0 || len > 64) return; // Keep bursts sane

    // Using a local buffer to ensure DMA compatibility without heap allocation
    // Note: Most LoRa buffers are small. If len > 64, use heap_caps_malloc.
    uint8_t tx_buf[len + 1]; 
    tx_buf[0] = reg;
    memcpy(&tx_buf[1], data, len);

    spi_transaction_t t = {};
    t.length = (len + 1) * 8;
    t.tx_buffer = tx_buf;

    spi_device_transmit(device_handle_, &t);
}

void Spi::read_burst(const uint8_t reg, uint8_t* buffer, const uint16_t len) noexcept 
{
    if (!isInitialized() || len == 0) return;

    // To read, we send the address, then the slave clocks out data
    uint8_t tx_addr = reg;
    
    spi_transaction_t t = {};
    t.length = (len + 1) * 8;
    t.tx_buffer = &tx_addr; 
    t.rx_buffer = buffer;   // Data starts at buffer[1]

    spi_device_transmit(device_handle_, &t);
}

// --- 16-BIT ADDRESS METHODS ---

uint8_t Spi::read_byte16(const uint16_t reg) noexcept 
{
    if (!isInitialized()) return 0x00;

    // We need 3 bytes: Addr High, Addr Low, and a Dummy byte to clock out the result
    // reg is handled externally for the R/W bit
    uint8_t tx_data[3] = {
        static_cast<uint8_t>(reg >> 8),   // MSB
        static_cast<uint8_t>(reg & 0xFF), // LSB
        0x00                              // Dummy byte to clock out the result
    };

    spi_transaction_t t = {};
    t.length = 24;                        // 24 bits total (16 addr + 8 data)
    t.tx_buffer = tx_data;
    t.flags = SPI_TRANS_USE_RXDATA;       // Direct store result in t.rx_data

    // Use polling to avoid FreeRTOS task switching overhead in your 1ms tick
    esp_err_t ret = spi_device_polling_transmit(device_handle_, &t);
    
    if (ret != ESP_OK) {
        return 0x00;
    }

    // t.rx_data[0] and [1] are the "junk" bytes received during the address phase
    // t.rx_data[2] contains the actual register value clocked out by the slave
    return t.rx_data[2];
}

void Spi::write_byte16(const uint16_t reg, const uint8_t data) noexcept 
{
    if (!isInitialized()) return;

    uint8_t tx_buf[3] = {
        static_cast<uint8_t>(reg >> 8),   // MSB
        static_cast<uint8_t>(reg & 0xFF), // LSB
        data                              // Data byte to write
    };

    spi_transaction_t t = {};
    t.length = 24; // 24 bits total (16 addr + 8 data)
    t.tx_buffer = tx_buf;

    spi_device_transmit(device_handle_, &t);
}

void Spi::write_burst16(const uint16_t reg, const uint8_t* data, const uint16_t len) noexcept 
{
    if (!isInitialized() || len == 0) return;

    uint8_t tx_buf[len + 2];
    tx_buf[0] = static_cast<uint8_t>(reg >> 8);
    tx_buf[1] = static_cast<uint8_t>(reg & 0xFF);
    memcpy(&tx_buf[2], data, len);

    spi_transaction_t t = {};
    t.length = (len + 2) * 8;
    t.tx_buffer = tx_buf;

    spi_device_transmit(device_handle_, &t);
}

void Spi::read_burst16(const uint16_t reg, uint8_t* buffer, const uint16_t len) noexcept 
{
    if (!isInitialized() || len == 0) return;

    uint8_t tx_addr[2] = { 
        static_cast<uint8_t>(reg >> 8), 
        static_cast<uint8_t>(reg & 0xFF) 
    };

    spi_transaction_t t = {};
    t.length = (len + 2) * 8;
    t.tx_buffer = tx_addr;
    t.rx_buffer = buffer; // Data starts at buffer[2]

    spi_device_transmit(device_handle_, &t);
}

} // namespace bsw