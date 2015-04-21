#ifndef BLUETOE_BITS_HPP
#define BLUETOE_BITS_HPP

#include <cstdint>

namespace bluetoe {
namespace details {

    inline std::uint16_t read_handle( const std::uint8_t* h )
    {
        return *h + ( *( h + 1 ) << 8 );
    }

    inline std::uint8_t* write_handle( std::uint8_t* out, std::uint16_t handle )
    {
        out[ 0 ] = handle & 0xff;
        out[ 1 ] = handle >> 8;

        return out + 2;
    }

    inline std::uint8_t* write_16bit_uuid( std::uint8_t* out, std::uint16_t uuid )
    {
        return write_handle( out, uuid );
    }


}
}
#endif
