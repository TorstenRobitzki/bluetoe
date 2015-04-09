#ifndef BLUETOE_SERVER_HPP
#define BLUETOE_SERVER_HPP

#include "codes.hpp"

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <iterator>

namespace bluetoe {

    template < typename ... Options >
    class server {
    public:
        void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
        {
            std::uint8_t error[] = { bits( att_opcodes::error_response ), input[ 0 ], 0x00, 0x00, bits( att_error_codes::invalid_pdu ) };
            std::copy( std::begin( error ), std::end( error ), output );
            out_size = sizeof( error ) / sizeof( error[ 0 ] );
        }
    };
}

#endif