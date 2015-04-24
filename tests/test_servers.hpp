#ifndef BLUETOE_TESTS_TEST_SERVERS_HPP
#define BLUETOE_TESTS_TEST_SERVERS_HPP

#include <bluetoe/server.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>

#include <initializer_list>
#include <vector>
#include <sstream>
#include <iomanip>
#include "hexdump.hpp"

namespace {
    std::uint16_t temperature_value = 0x0104;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
                bluetoe::bind_characteristic_value< decltype( temperature_value ), &temperature_value >,
                bluetoe::no_write_access
            >
        >
    > small_temperature_service;

    /*
     * Example with 3 characteristics, the example contains 7 attributes.
     */
    std::uint8_t ape1 = 1, ape2 = 2, ape3 = 3;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
                bluetoe::bind_characteristic_value< std::uint8_t, &ape1 >
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAB >,
                bluetoe::bind_characteristic_value< std::uint8_t, &ape2 >
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAC >,
                bluetoe::bind_characteristic_value< std::uint8_t, &ape3 >
            >
        >
    > three_apes_service;

    template < class Server, std::size_t ResponseBufferSize = 23 >
    struct request_with_reponse : Server
    {
        request_with_reponse()
            : response_size( ResponseBufferSize )
            , connection( ResponseBufferSize )
        {
            std::fill( std::begin( response ), std::end( response ), 0x55 );
            connection.client_mtu( ResponseBufferSize );
        }

        template < std::size_t PDU_Size >
        void l2cap_input( const std::uint8_t(&input)[PDU_Size] )
        {
            Server::l2cap_input( input, PDU_Size, response, response_size, connection );
            BOOST_REQUIRE_LE( response_size, ResponseBufferSize );
        }

        void l2cap_input( const std::initializer_list< std::uint8_t >& input )
        {
            const std::vector< std::uint8_t > values( input );

            Server::l2cap_input( &values[ 0 ], values.size(), response, response_size, connection );
            BOOST_REQUIRE_LE( response_size, ResponseBufferSize );
        }

        void expected_result( const std::initializer_list< std::uint8_t >& input )
        {
            const std::vector< std::uint8_t > values( input );
            BOOST_REQUIRE_EQUAL_COLLECTIONS( values.begin(), values.end(), &response[ 0 ], &response[ response_size ] );
        }

        template < std::size_t PDU_Size >
        bool check_error_response( const std::uint8_t(&input)[PDU_Size], std::uint8_t expected_request_opcode, std::uint16_t expected_attribute_handle, std::uint8_t expected_error_code )
        {
            return check_error_response_impl( &input[ 0 ], PDU_Size, expected_request_opcode, expected_attribute_handle, expected_error_code );
        }

        bool check_error_response( const std::initializer_list< std::uint8_t >& input, std::uint8_t expected_request_opcode, std::uint16_t expected_attribute_handle, std::uint8_t expected_error_code )
        {
            const std::vector< std::uint8_t > values( input );
            return check_error_response_impl( &values[ 0 ], values.size(), expected_request_opcode, expected_attribute_handle, expected_error_code );
        }

        void dump()
        {
            hex_dump( std::cout, &response[ 0 ], &response[ response_size ] );
        }

        static_assert( ResponseBufferSize >= 23, "min MTU size is 23, no point in using less" );

        std::uint8_t                        response[ ResponseBufferSize ];
        std::size_t                         response_size;
        typename Server::connection_data    connection;
    private:
        template < typename T >
        std::string param_to_text( const T& param )
        {
            std::ostringstream out;
            out << "0x" << std::hex << param;

            return out.str();
        }

        std::string param_to_text( std::uint8_t c )
        {
            return param_to_text( static_cast< int >( c ) );
        }

        template < typename T, typename U >
        std::string should_be_but( const char* text, const T& should, const U& but )
        {
            std::ostringstream out;
            out << "ATT: \"" << text << "\" should be " << param_to_text( should ) << " but is: " <<  param_to_text( but );
            return out.str();
        }

        bool check_error_response_impl( std::uint8_t const * input, std::size_t input_size, std::uint8_t expected_request_opcode, std::uint16_t expected_attribute_handle, std::uint8_t expected_error_code )
        {
            std::uint8_t output[ ResponseBufferSize ];
            std::size_t  outlength = sizeof( output );

            Server::l2cap_input( input, input_size, output, outlength, connection );

            const std::uint8_t  opcode           = output[ 0 ];
            const std::uint8_t  request_opcode   = output[ 1 ];
            const std::uint16_t attribute_handle = output[ 2 ] + ( output[ 3 ] << 8 );
            const std::uint8_t  error_code       = output[ 4 ];

            BOOST_CHECK_MESSAGE( outlength == 5, should_be_but( "PDU Size", 5, outlength ) );
            BOOST_CHECK_MESSAGE( opcode == 0x01, should_be_but( "Attribute Opcode", 0x01, opcode ) );
            BOOST_CHECK_MESSAGE( request_opcode == expected_request_opcode, should_be_but( "Request Opcode In Error", expected_request_opcode, request_opcode ) );
            BOOST_CHECK_MESSAGE( attribute_handle == expected_attribute_handle, should_be_but( "Attribute Handle In Error", expected_attribute_handle, attribute_handle ) );
            BOOST_CHECK_MESSAGE( error_code == expected_error_code, should_be_but( "Error Code", expected_error_code, error_code ) );

            return outlength == 5
                && opcode == 0x01
                && request_opcode == expected_request_opcode
                && attribute_handle == expected_attribute_handle
                && error_code == expected_error_code;
        }

    };

    template < std::size_t ResponseBufferSize = 23 >
    using small_temperature_service_with_response = request_with_reponse< small_temperature_service, ResponseBufferSize >;

}

#endif
