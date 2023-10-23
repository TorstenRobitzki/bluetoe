#ifndef BLUTOE_TESTS_SCHEDULED_RADIO_NRF_UART_STREAM_HPP
#define BLUTOE_TESTS_SCHEDULED_RADIO_NRF_UART_STREAM_HPP

#include <nrf.h>

#include <cstdint>

template < std::uint32_t RxPin, std::uint32_t TxPin, std::uint32_t CtsPin, std::uint32_t RtsPin >
class nrf_uart_stream
{
public:
    void put( std::uint8_t );
    std::uint8_t get();
};

#endif
