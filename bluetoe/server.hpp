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

    namespace details {
        struct server_name_meta_type;
    }

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
        std::size_t advertising_data( std::uint8_t* buffer, std::size_t buffer_size );
    private:
        typedef typename details::find_all_by_meta_type< details::service_meta_type, Options... >::type services;

        static constexpr std::size_t number_of_attributes =
            details::sum_up_attributes< services >::value;

        static_assert( std::tuple_size< services >::value > 0, "A server should at least contain one service." );

        static details::attribute attribute_at( std::size_t index );

        void error_response( details::att_opcodes opcode, details::att_error_codes error_code, std::uint16_t handle, std::uint8_t* output, std::size_t& out_size );
        void error_response( details::att_opcodes opcode, details::att_error_codes error_code, std::uint8_t* output, std::size_t& out_size );
        void handle_find_information_request( details::att_opcodes opcode, const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );
        void handle_read_by_group_type_request( details::att_opcodes opcode, const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );

        std::uint8_t* collect_handle_uuid_tuples( std::uint16_t start, std::uint16_t end, bool only_16_bit, std::uint8_t* output, std::uint8_t* output_end );
        static std::uint16_t read_handle( const std::uint8_t* );
        static void write_handle( std::uint8_t* out, std::uint16_t handle );
        static void write_16bit_uuid( std::uint8_t* out, std::uint16_t uuid );
        static void write_128bit_uuid( std::uint8_t* out, const details::attribute& char_declaration );
    };

    /**
     * @brief adds additional options to a given server definition
     *
     * example:
     * @code
    unsigned temperature_value = 0;

    typedef bluetoe::server<
    ...
    > small_temperature_service;

    typedef bluetoe::extend_server<
        small_temperature_service,
        bluetoe::server_name< name >
    > small_named_temperature_service;

     * @endcode
     * @sa server
     */
    template < typename Server, typename ... Options >
    struct extend_server;

    template < typename ... ServerOptions, typename ... Options >
    struct extend_server< server< ServerOptions... >, Options... > : server< ServerOptions..., Options... >
    {
    };

    /**
     * @brief adds a discoverable device name
     */
    template < const char* const Name >
    struct server_name {
        typedef details::server_name_meta_type meta_type;

        static constexpr char const* name = Name;
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
        case details::att_opcodes::read_by_group_type_request:
            handle_read_by_group_type_request( opcode, input, in_size, output, out_size );
            break;
        default:
            error_response( opcode, details::att_error_codes::invalid_pdu, output, out_size );
            break;
        }
    }

    template < typename ... Options >
    std::size_t server< Options... >::advertising_data( std::uint8_t* begin, std::size_t buffer_size )
    {
        std::uint8_t* const end = begin + buffer_size;

        if ( buffer_size >= 3 )
        {
            begin[ 0 ] = 2;
            begin[ 1 ] = bits( details::gap_types::flags );
            // LE General Discoverable Mode | BR/EDR Not Supported
            begin[ 2 ] = 6;

            begin += 3;
        }

        typedef typename details::find_by_meta_type< details::server_name_meta_type, Options..., server_name< nullptr > >::type name;

        if ( name::name && ( end - begin ) > 2 )
        {
            const std::size_t name_length  = name::name ? std::strlen( name::name ) : 0u;
            const std::size_t max_name_len = std::min< std::size_t >( name_length, end - begin - 2 );

            if ( name_length > 0 )
            {
                begin[ 0 ] = max_name_len + 1;
                begin[ 1 ] = max_name_len == name_length
                    ? bits( details::gap_types::complete_local_name )
                    : bits( details::gap_types::shortened_local_name );

                std::copy( name::name + 0, name::name + max_name_len, &begin[ 2 ] );
                begin += max_name_len + 2;
            }
        }

        return buffer_size - ( end - begin );
    }

    template < typename ... Options >
    details::attribute server< Options... >::attribute_at( std::size_t index )
    {
        return details::attribute_at_list< services >::attribute_at( index );
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

        if ( starting_handle > number_of_attributes )
            return error_response( opcode, details::att_error_codes::attribute_not_found, starting_handle, output, out_size );

        const bool only_16_bit_uuids = attribute_at( starting_handle -1 ).uuid != bits( details::gatt_uuids::internal_128bit_uuid );

        std::uint8_t*        write_ptr = &output[ 0 ];
        std::uint8_t* const  write_end = write_ptr + out_size;

        *write_ptr = bits( details::att_opcodes::find_information_response );
        ++write_ptr;

        if ( write_ptr != write_end )
        {
            *write_ptr = bits(
                only_16_bit_uuids
                    ? details::att_uuid_format::short_16bit
                    : details::att_uuid_format::long_128bit );

            ++write_ptr;

        }

        write_ptr = collect_handle_uuid_tuples( starting_handle, ending_handle, only_16_bit_uuids, write_ptr, write_end );

        out_size = write_ptr - &output[ 0 ];
    }

    template < typename ... Options >
    void server< Options... >::handle_read_by_group_type_request( details::att_opcodes opcode, const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        if ( in_size != 5 + 2 && in_size != 5 + 16  )
            return error_response( opcode, details::att_error_codes::invalid_pdu, output, out_size );

        const std::uint16_t starting_handle = read_handle( &input[ 1 ] );
        const std::uint16_t ending_handle   = read_handle( &input[ 3 ] );

        if ( starting_handle == 0 || starting_handle > ending_handle )
            return error_response( opcode, details::att_error_codes::invalid_handle, starting_handle, output, out_size );

        if ( starting_handle > number_of_attributes )
            return error_response( opcode, details::att_error_codes::attribute_not_found, starting_handle, output, out_size );

        return error_response( opcode, details::att_error_codes::unsupported_group_type, starting_handle, output, out_size );
    }

    template < typename ... Options >
    std::uint8_t* server< Options... >::collect_handle_uuid_tuples( std::uint16_t start, std::uint16_t end, bool only_16_bit, std::uint8_t* out, std::uint8_t* out_end )
    {
        const std::size_t size_per_tuple = only_16_bit
            ? 2 + 2
            : 2 + 16;

        for ( ; start <= end && start <= number_of_attributes && out_end - out >= size_per_tuple; ++start )
        {
            const details::attribute attr = attribute_at( start -1 );
            const bool is_16_bit_uuids    = attr.uuid != bits( details::gatt_uuids::internal_128bit_uuid );

            if ( only_16_bit == is_16_bit_uuids )
            {
                write_handle( out, start );

                if ( is_16_bit_uuids )
                {
                    write_16bit_uuid( out + 2, attr.uuid );
                }
                else
                {
                    write_128bit_uuid( out + 2, attribute_at( start -2 ) );
                }

                out += size_per_tuple;
            }
        }

        return out;
    }

    template < typename ... Options >
    std::uint16_t server< Options... >::read_handle( const std::uint8_t* h )
    {
        return *h + ( *( h + 1 ) << 8 );
    }

    template < typename ... Options >
    void server< Options... >::write_handle( std::uint8_t* out, std::uint16_t handle )
    {
        out[ 0 ] = handle & 0xff;
        out[ 1 ] = handle >> 8;
    }

    template < typename ... Options >
    void server< Options... >::write_16bit_uuid( std::uint8_t* out, std::uint16_t uuid )
    {
        write_handle( out, uuid );
    }

    template < typename ... Options >
    void server< Options... >::write_128bit_uuid( std::uint8_t* out, const details::attribute& char_declaration )
    {
        // this is a little bit tricky: To save memory, details::attribute contains only 16 bit uuids as all
        // but the "Characteristic Value Declaration" contain 16 bit uuids. However, as the "Characteristic Value Declaration"
        // "is the first Attribute after the characteristic declaration", the attribute just in front of the
        // "Characteristic Value Declaration" contains the the 128 bit uuid.
        assert( char_declaration.uuid == bits( details::gatt_uuids::characteristic ) );

        std::uint8_t buffer[ 3 + 16 ];
        auto read = details::attribute_access_arguments::read( buffer );
        char_declaration.access( read );

        assert( read.buffer_size == sizeof( buffer ) );

        std::copy( &read.buffer[ 3 ], &read.buffer[ 3 + 16 ], out );
    }

}

#endif