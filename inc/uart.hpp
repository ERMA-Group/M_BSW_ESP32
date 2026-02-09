/**
 * @file uart.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>

#include "driver/uart.h"
#include "driver/gpio.h"

namespace bsw {

/**
 * @brief UART communication class for ESP32.
 */

class Uart {
public:
    enum class Module : uint8_t {
        kUart0 = static_cast<uint8_t>(uart_port_t::UART_NUM_0),
        kUart1 = static_cast<uint8_t>(uart_port_t::UART_NUM_1),
        kUart2 = static_cast<uint8_t>(uart_port_t::UART_NUM_2),
    };

    enum class DataBits : uint8_t {
        kDataBits5 = static_cast<uint8_t>(uart_word_length_t::UART_DATA_5_BITS),
        kDataBits6 = static_cast<uint8_t>(uart_word_length_t::UART_DATA_6_BITS),
        kDataBits7 = static_cast<uint8_t>(uart_word_length_t::UART_DATA_7_BITS),
        kDataBits8 = static_cast<uint8_t>(uart_word_length_t::UART_DATA_8_BITS),
    };

    enum class StopBits : uint8_t {
        kStopBits1   = static_cast<uint8_t>(uart_stop_bits_t::UART_STOP_BITS_1),
        kStopBits1_5 = static_cast<uint8_t>(uart_stop_bits_t::UART_STOP_BITS_1_5),
        kStopBits2   = static_cast<uint8_t>(uart_stop_bits_t::UART_STOP_BITS_2),
    };

    enum class Parity : uint8_t {
        kParityDisable = static_cast<uint8_t>(uart_parity_t::UART_PARITY_DISABLE),
        kParityEven    = static_cast<uint8_t>(uart_parity_t::UART_PARITY_EVEN),
        kParityOdd     = static_cast<uint8_t>(uart_parity_t::UART_PARITY_ODD),
    };
    struct Config {
        Module module;
        DataBits data_bits;
        StopBits stop_bits;
        Parity parity;
        uint8_t tx_pin;
        uint8_t rx_pin;
        uint32_t baud_rate;
        uint16_t rx_buf_size = 1024;
    };

    static constexpr uint32_t kRxWaitTimeoutMs = 0;  ///< Timeout for receiving data in milliseconds

    /**
     * @brief Constructor for Uart class.
     */
    Uart() noexcept = default;
    Uart(const Config& config) noexcept;

    ~Uart() noexcept = default;

    /**
     * @brief Initialize the UART interface.
     * @return True if initialization was successful, false otherwise.
     */
    bool init() noexcept;

    /**
     * @brief Send data over UART.
     * @param data Pointer to the data buffer to send.
     * @param length Length of the data buffer.
     * @return Number of bytes sent.
     */
    uint8_t send(const uint8_t* data, size_t length) noexcept;

    /**
     * @brief Send a single byte over UART.
     * @param data Byte to send.
     * @return True if the byte was sent successfully, false otherwise.
     */
    bool send_byte(const uint8_t data) noexcept;

    /**
     * @brief Receive data from UART - call cyclically.
     * @param buffer Pointer to the buffer to store received data.
     * @param length Maximum length of data to receive.
     * @return Number of bytes received.
     */
    uint16_t receive(uint8_t* buffer, size_t length) noexcept;

    bool isInitialized() const noexcept { return initialized_; }

private:
    Config config_;
    bool initialized_ = false;
};

} // namespace bsw