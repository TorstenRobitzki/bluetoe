#include "link/crc16.hpp"

namespace bluetoe {
namespace test_rig {

    // bit by bit without a table: the link is slow compared to the core and a table would
    // only cost flash
    std::uint16_t crc16( const std::uint8_t* data, std::size_t size, std::uint16_t initial )
    {
        std::uint16_t value = initial;

        for ( std::size_t i = 0; i != size; ++i )
        {
            value = static_cast< std::uint16_t >( value ^ ( static_cast< std::uint16_t >( data[ i ] ) << 8 ) );

            for ( int bit = 0; bit != 8; ++bit )
                value = static_cast< std::uint16_t >( ( value & 0x8000 ) ? ( value << 1 ) ^ 0x1021 : value << 1 );
        }

        return value;
    }
}
}
