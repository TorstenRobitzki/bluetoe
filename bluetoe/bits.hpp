#ifndef BLUETOE_BITS_HPP
#define BLUETOE_BITS_HPP

#include <cstdint>
#include <bluetoe/codes.hpp>

namespace bluetoe {
namespace details {

    constexpr std::uint16_t read_handle( const std::uint8_t* h )
    {
        return *h + ( *( h + 1 ) << 8 );
    }

    constexpr std::uint16_t read_16bit_uuid( const std::uint8_t* h )
    {
        return read_handle( h );
    }

    constexpr std::uint16_t read_16bit( const std::uint8_t* h )
    {
        return read_handle( h );
    }

    constexpr std::uint32_t read_32bit( const std::uint8_t* p )
    {
        return static_cast< std::uint32_t >( read_16bit( p ) )
             | ( static_cast< std::uint32_t >( read_16bit( p + 2 ) ) << 16 );
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

    inline std::uint8_t* write_16bit( std::uint8_t* out, std::uint16_t bits16 )
    {
        return write_handle( out, bits16 );
    }

    inline std::uint8_t* write_32bit( std::uint8_t* out, std::uint32_t bits32 )
    {
        return write_16bit( write_16bit( out, bits32 & 0xffff ), bits32 >> 16 );
    }

    inline std::uint8_t* write_opcode( std::uint8_t* out, details::att_opcodes opcode )
    {
        *out = bits( opcode );
        return out + 1;
    }

    inline std::uint8_t* write_byte( std::uint8_t* out, std::uint8_t byte )
    {
        *out = byte;
        return out + 1;
    }

}
}
#endif
