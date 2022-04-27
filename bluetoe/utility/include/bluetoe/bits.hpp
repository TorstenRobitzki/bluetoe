#ifndef BLUETOE_BITS_HPP
#define BLUETOE_BITS_HPP

#include <cstdint>

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

    constexpr std::uint32_t read_24bit( const std::uint8_t* h )
    {
        return static_cast< std::uint32_t >( read_16bit( h ) ) | ( static_cast< std::uint32_t >( *( h + 2 ) ) << 16 );
    }

    constexpr std::uint32_t read_32bit( const std::uint8_t* p )
    {
        return static_cast< std::uint32_t >( read_16bit( p ) )
             | ( static_cast< std::uint32_t >( read_16bit( p + 2 ) ) << 16 );
    }

    constexpr std::uint64_t read_64bit( const std::uint8_t* p )
    {
        return static_cast< std::uint64_t >( read_32bit( p ) )
             | ( static_cast< std::uint64_t >( read_32bit( p + 4 ) ) << 32 );
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

    inline std::uint8_t* write_64bit( std::uint8_t* out, std::uint64_t bits64 )
    {
        return write_32bit( write_32bit( out, bits64 & 0xffffffff ), bits64 >> 32 );
    }

    inline std::uint8_t* write_byte( std::uint8_t* out, std::uint8_t byte )
    {
        *out = byte;
        return out + 1;
    }

}
}
#endif
