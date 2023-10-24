#ifndef BLUTOE_TESTS_SCHEDULED_RADIO_NRF_UART_STREAM_HPP
#define BLUTOE_TESTS_SCHEDULED_RADIO_NRF_UART_STREAM_HPP

#include <nrf.h>

#include <cstdint>

template < std::uint32_t RxPin, std::uint32_t TxPin, std::uint32_t CtsPin, std::uint32_t RtsPin >
class nrf_uart_stream
{
public:
    void put( std::uint8_t );
    void put( const std::uint8_t* p, std::size_t len );
    std::uint8_t get();
    void get( std::uint8_t* p, std::size_t len );
};

#endif
