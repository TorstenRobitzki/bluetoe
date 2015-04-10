#ifndef BLUETOE_SERVER_HPP
#define BLUETOE_SERVER_HPP

#include <bluetoe/codes.hpp>

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <iterator>
#include <cassert>

namespace bluetoe {

    /**
     * @brief implementation of an GATT server
     *
     * The server serves one or more services configured by the given Options.
     */
    template < typename ... Options >
    class server {
    public:
        void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );

    private:
        void error_response( att_opcodes opcode, att_error_codes error_code, std::uint16_t handle, std::uint8_t* output, std::size_t& out_size );
        void error_response( att_opcodes opcode, att_error_codes error_code, std::uint8_t* output, std::size_t& out_size );
        void handle_find_information_request( att_opcodes opcode, const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );
        static std::uint16_t read_handle( const std::uint8_t* );
    };


    template < typename ... Options >
    void server< Options... >::l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        assert( in_size != 0 );
        assert( out_size >= 23 );

        const att_opcodes opcode = static_cast< att_opcodes >( input[ 0 ] );

        switch ( opcode )
        {
        case att_opcodes::find_information_request:
            handle_find_information_request( opcode, input, in_size, output, out_size );
            break;
        default:
            error_response( opcode, att_error_codes::invalid_pdu, output, out_size );
            break;
        }
    }

    template < typename ... Options >
    void server< Options... >::error_response( att_opcodes opcode, att_error_codes error_code, std::uint16_t handle, std::uint8_t* output, std::size_t& out_size )
    {
        if ( out_size >= 5 )
        {
            output[ 0 ] = bits( att_opcodes::error_response );
            output[ 1 ] = bits( opcode );
            output[ 2 ] = handle & 0xff;
            output[ 3 ] = handle << 8;
            output[ 4 ] = bits( error_code );
            out_size = 5;
        }
        else
        {
            out_size = 0 ;
        }
    }

    template < typename ... Options >
    void server< Options... >::error_response( att_opcodes opcode, att_error_codes error_code, std::uint8_t* output, std::size_t& out_size )
    {
        error_response( opcode, error_code, 0, output, out_size );
    }

    template < typename ... Options >
    void server< Options... >::handle_find_information_request( att_opcodes opcode, const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        if ( in_size != 5 )
            return error_response( opcode, att_error_codes::invalid_pdu, output, out_size );

        const std::uint16_t starting_handle = read_handle( &input[ 1 ] );
        const std::uint16_t ending_handle   = read_handle( &input[ 3 ] );

        if ( starting_handle == 0 || starting_handle > ending_handle )
            return error_response( opcode, att_error_codes::invalid_handle, starting_handle, output, out_size );

        out_size = 20;
        output[ 0 ] = bits( att_opcodes::find_information_response );
        output[ 1 ] = bits( att_uuid_format::long_128bit );
    }

    template < typename ... Options >
    std::uint16_t server< Options... >::read_handle( const std::uint8_t* h )
    {
        return *h + ( *( h + 1 ) << 8 );
    }

}

#endif