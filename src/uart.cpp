/**
 * @file uart.cpp
 * @brief C++ class implementation
 */

#include "uart.hpp"   // C++ class definition

namespace bsw {

Uart::Uart(const Config& config) noexcept
    : config_(config)
{
}

bool Uart::init() noexcept
{
    uart_config_t uart_config = {
        .baud_rate = static_cast<int>(config_.baud_rate),
        .data_bits = static_cast<uart_word_length_t>(config_.data_bits),
        .parity    = static_cast<uart_parity_t>(config_.parity),
        .stop_bits = static_cast<uart_stop_bits_t>(config_.stop_bits),
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT, // Standard for ESP32
    };

    auto port = static_cast<uart_port_t>(config_.module);

    /* Configure UART parameters */
    if (uart_param_config(port, &uart_config) != ESP_OK)
    {
        initialized_ = false;
        return false;
    }

    /* Install UART driver */
    if (uart_driver_install(port, config_.rx_buf_size * 2, 0, 0, nullptr, 0) != ESP_OK)
    {
        initialized_ = false;
        return false;
    }

    /* Set UART pins */
    esp_err_t pin_err;
    if (config_.rx_pin == 3 || config_.rx_pin == 1)
    {
        pin_err = uart_set_pin(port, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, 
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    else
    {
        pin_err = uart_set_pin(port, config_.tx_pin, config_.rx_pin, 
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    initialized_ = (pin_err == ESP_OK);
    return (pin_err == ESP_OK);
}

uint8_t Uart::send(const uint8_t* data, size_t length) noexcept
{
    int bytes_sent { uart_write_bytes(static_cast<uart_port_t>(config_.module), reinterpret_cast<const char*>(data), static_cast<size_t>(length)) };
    if (bytes_sent < 0)
    {
        return 0;
    }
    return static_cast<uint8_t>(bytes_sent);
}

bool Uart::send_byte(const uint8_t data) noexcept
{
    int bytes_sent { uart_write_bytes(static_cast<uart_port_t>(config_.module), reinterpret_cast<const char*>(&data), 1) };
    return (bytes_sent == 1);
}

uint16_t Uart::receive(uint8_t* buffer, size_t length) noexcept
{
    int bytes_received { uart_read_bytes(static_cast<uart_port_t>(config_.module), buffer, static_cast<size_t>(length), pdMS_TO_TICKS(kRxWaitTimeoutMs)) };
    if (bytes_received < 0) 
    {
        return 0;
    }
    return static_cast<uint16_t>(bytes_received);
}
}  // namespace bsw