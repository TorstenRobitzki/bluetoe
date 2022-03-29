#ifndef BLUETOE_TESTS_TEST_TOOLS_TEST_LAYOUT_HPP
#define BLUETOE_TESTS_TEST_TOOLS_TEST_LAYOUT_HPP

#include <bluetoe/buffer.hpp>
#include <bluetoe/bits.hpp>

#include <cstddef>
#include <cstdint>

namespace test {
    template < std::size_t Overhead >
    struct layout_with_overhead
    {
        static constexpr std::size_t header_size = sizeof( std::uint16_t );

        static std::uint16_t header( const std::uint8_t* pdu )
        {
            return ::bluetoe::details::read_16bit( pdu );
        }

        static void header( std::uint8_t* pdu, std::uint16_t header_value )
        {
            ::bluetoe::details::write_16bit( pdu, header_value );
        }

        static std::uint16_t header( const bluetoe::read_buffer& pdu )
        {
            assert( pdu.size >= data_channel_pdu_memory_size( 0 ) );

            return header( pdu.buffer );
        }

        static std::uint16_t header( const bluetoe::write_buffer& pdu )
        {
            assert( pdu.size >= data_channel_pdu_memory_size( 0 ) );

            return header( pdu.buffer );
        }

        static void header( const bluetoe::read_buffer& pdu, std::uint16_t header_value )
        {
            assert( pdu.size >= data_channel_pdu_memory_size( 0 ) );

            header( pdu.buffer, header_value );
        }

        static std::pair< std::uint8_t*, std::uint8_t* > body( const bluetoe::read_buffer& pdu )
        {
            assert( pdu.size >= header_size );

            return { &pdu.buffer[ header_size + Overhead ], &pdu.buffer[ pdu.size ] };
        }

        static std::pair< const std::uint8_t*, const std::uint8_t* > body( const bluetoe::write_buffer& pdu )
        {
            assert( pdu.size >= header_size );

            return { &pdu.buffer[ header_size + Overhead ], &pdu.buffer[ pdu.size ] };
        }

        static constexpr std::size_t data_channel_pdu_memory_size( std::size_t payload_size )
        {
            return header_size + Overhead + payload_size;
        }
    };
}

#endif
