#ifndef BLUTOE_TESTS_SCHEDULED_RADIO_NRF_UART_STREAM_HPP
#define BLUTOE_TESTS_SCHEDULED_RADIO_NRF_UART_STREAM_HPP

#include <nrf.h>

#include <cstdint>

template < std::uint32_t RxPin, std::uint32_t TxPin, std::uint32_t CtsPin, std::uint32_t RtsPin >
class nrf_uart_stream
{
public:
    nrf_uart_stream()
        : uarte_( *NRF_UARTE1 )
    {
        NRF_P0->PIN_CNF[ RxPin ] =
            ( GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos )
          | ( GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos )
          | ( GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos );

        NRF_P0->PIN_CNF[ CtsPin ] =
            ( GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos )
          | ( GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos )
          | ( GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos );

        NRF_P0->OUTSET = 1 << TxPin;

        NRF_P0->PIN_CNF[ TxPin ] =
            ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos )
          | ( GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos )
          | ( GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos )
          | ( GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos );

        NRF_P0->OUTSET = 1 << RtsPin;

        NRF_P0->PIN_CNF[ RtsPin ] =
            ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos )
          | ( GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos )
          | ( GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos )
          | ( GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos );

        uarte_.ENABLE    = ( UARTE_ENABLE_ENABLE_Enabled << UARTE_ENABLE_ENABLE_Pos );
        uarte_.CONFIG    = ( UARTE_CONFIG_HWFC_Enabled << UARTE_CONFIG_HWFC_Pos );

        uarte_.BAUDRATE  = UART_BAUDRATE_BAUDRATE_Baud115200;
        uarte_.PSEL.RTS  = RtsPin;
        uarte_.PSEL.TXD  = TxPin;
        uarte_.PSEL.CTS  = CtsPin;
        uarte_.PSEL.RXD  = RxPin;
    }

    void put( std::uint8_t c )
    {
        uarte_.EVENTS_ENDTX = 0;
        uarte_.TXD.MAXCNT = 1;
        uarte_.TXD.PTR    = reinterpret_cast< std::uint32_t >( &c );

        uarte_.TASKS_STARTTX = 1;

        while ( uarte_.EVENTS_ENDTX == 0 )
            ;
    }

    void put( const std::uint8_t* p, std::size_t len )
    {
        for ( std::size_t i = 0; i != len; ++i )
            put( p[ i ] );
    }

    std::uint8_t get()
    {
        std::uint8_t result = 0;

        uarte_.EVENTS_ENDRX = 0;
        uarte_.RXD.MAXCNT = 1;
        uarte_.RXD.PTR    = reinterpret_cast< std::uint32_t >( &result );

        uarte_.TASKS_STARTRX = 1;

        while ( uarte_.EVENTS_ENDRX == 0 )
            ;

        return result;
    }

    void get( std::uint8_t* p, std::size_t len )
    {
        for ( std::size_t i = 0; i != len; ++i )
            p[ i ] = get();
    }

    void flush()
    {
    }

private:
    NRF_UARTE_Type& uarte_;
};

#endif
