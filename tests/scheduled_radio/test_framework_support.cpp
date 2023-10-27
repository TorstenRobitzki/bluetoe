#include "test_framework_support.hpp"

#include "system_nrf52.h"
#include "tester_config.hpp"
#include <nrf.h>

#include <cassert>
#include <streambuf>
#include <ostream>

extern "C" void _close(void) { assert( false ); }
extern "C" void _fstat(void) { assert( false ); }
extern "C" void _getpid(void) { assert( false ); }
extern "C" void _gettimeofday(void) { assert( false ); }
extern "C" void _isatty(void) { assert( false ); }
extern "C" void _kill(void) { assert( false ); }
extern "C" void _lseek(void) { assert( false ); }
extern "C" void _open(void) { assert( false ); }
extern "C" void _read(void) { assert( false ); }
extern "C" void _write(void) { assert( false ); }


template < std::uint32_t TxPin >
class uart_stream_buf : public std::basic_streambuf< char >
{
public:
    uart_stream_buf()
        : uarte_( *NRF_UARTE1 )
    {
        NRF_P0->OUTSET = 1 << TxPin;

        NRF_P0->PIN_CNF[ TxPin ] =
            ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos )
          | ( GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos )
          | ( GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos )
          | ( GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos );

        uarte_.ENABLE    = ( UARTE_ENABLE_ENABLE_Enabled << UARTE_ENABLE_ENABLE_Pos );

        uarte_.BAUDRATE  = UART_BAUDRATE_BAUDRATE_Baud115200;
        uarte_.PSEL.TXD  = TxPin;

        setp( std::begin( buffer_ ), std::end( buffer_ ) );
    }

private:
    virtual int_type overflow(int_type ch) override
    {
        uarte_.EVENTS_ENDTX = 0;

        uarte_.TXD.MAXCNT = pptr() - pbase();
        uarte_.TXD.PTR    = reinterpret_cast< std::uint32_t >( &buffer_[ 0 ] );

        uarte_.TASKS_STARTTX = 1;

        while ( uarte_.EVENTS_ENDTX == 0 )
            ;

        setp( std::begin( buffer_ ), std::end( buffer_ ) );

        return std::char_traits< char >::to_char_type( sputc( ch ) );
    }

    NRF_UARTE_Type& uarte_ = *NRF_UARTE1;
    char buffer_[ 16 ];
};

std::ostream& Catch::cout()
{
    static uart_stream_buf< loggin_uart_transmit_pin > buf;
    static std::ostream out( &buf );

    return out;
}

std::ostream& Catch::cerr()
{
    return Catch::cout();
}

std::ostream& Catch::clog()
{
    return Catch::cout();
}
