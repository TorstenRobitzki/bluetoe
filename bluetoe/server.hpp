#ifndef BLUETOE_SERVER_HPP
#define BLUETOE_SERVER_HPP

#include <bluetoe/codes.hpp>
#include <bluetoe/service.hpp>

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <iterator>
#include <cassert>

namespace bluetoe {

    /**
     * @brief Root of the declaration of a GATT server.
     *
     * The server serves one or more services configured by the given Options. To configure the server, pass one or more bluetoe::service types as parameters.
     *
     * example:
     * @code
    unsigned temperature_value = 0;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
            bluetoe::characteristic<
                bluetoe::bind_characteristic_value< decltype( temperature_value ), &temperature_value >,
                bluetoe::no_write_access
            >
        >
    > small_temperature_service;
     * @endcode
     * @sa service
     */
    template < typename ... Options >
    class server {
    public:
        void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );

    private:
        static details::attribute attribute_at( std::size_t index );

        typedef typename details::find_all_by_meta_type< details::service_meta_type, Options... >::type services;

        static_assert( std::tuple_size< services >::value > 0, "A server should at least contain one service." );
        void error_response( details::att_opcodes opcode, details::att_error_codes error_code, std::uint16_t handle, std::uint8_t* output, std::size_t& out_size );
        void error_response( details::att_opcodes opcode, details::att_error_codes error_code, std::uint8_t* output, std::size_t& out_size );
        void handle_find_information_request( details::att_opcodes opcode, const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );
        static std::uint16_t read_handle( const std::uint8_t* );
    };

    /*
     * Implementation
     */
    template < typename ... Options >
    void server< Options... >::l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        assert( in_size != 0 );
        assert( out_size >= 23 );

        const details::att_opcodes opcode = static_cast< details::att_opcodes >( input[ 0 ] );

        switch ( opcode )
        {
        case details::att_opcodes::find_information_request:
            handle_find_information_request( opcode, input, in_size, output, out_size );
            break;
        default:
            error_response( opcode, details::att_error_codes::invalid_pdu, output, out_size );
            break;
        }
    }

    template < typename ... Options >
    void server< Options... >::error_response( details::att_opcodes opcode, details::att_error_codes error_code, std::uint16_t handle, std::uint8_t* output, std::size_t& out_size )
    {
        if ( out_size >= 5 )
        {
            output[ 0 ] = bits( details::att_opcodes::error_response );
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
    void server< Options... >::error_response( details::att_opcodes opcode, details::att_error_codes error_code, std::uint8_t* output, std::size_t& out_size )
    {
        error_response( opcode, error_code, 0, output, out_size );
    }

    template < typename ... Options >
    void server< Options... >::handle_find_information_request( details::att_opcodes opcode, const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        if ( in_size != 5 )
            return error_response( opcode, details::att_error_codes::invalid_pdu, output, out_size );

        const std::uint16_t starting_handle = read_handle( &input[ 1 ] );
        const std::uint16_t ending_handle   = read_handle( &input[ 3 ] );

        if ( starting_handle == 0 || starting_handle > ending_handle )
            return error_response( opcode, details::att_error_codes::invalid_handle, starting_handle, output, out_size );

        out_size = 20;
        output[ 0 ] = bits( details::att_opcodes::find_information_response );
        output[ 1 ] = bits( details::att_uuid_format::long_128bit );
    }

    template < typename ... Options >
    std::uint16_t server< Options... >::read_handle( const std::uint8_t* h )
    {
        return *h + ( *( h + 1 ) << 8 );
    }

}

#endif