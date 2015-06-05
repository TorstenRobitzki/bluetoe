#ifndef BLUETOE_TESTS_TEST_SERVERS_HPP
#define BLUETOE_TESTS_TEST_SERVERS_HPP

#include <bluetoe/server.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>

#include <initializer_list>
#include <vector>
#include <sstream>
#include <iostream>
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

            notification = bluetoe::details::notification_data();
            this->notification_callback( &l2cap_layer_notify_cb );
        }

        template < std::size_t PDU_Size >
        void l2cap_input( const std::uint8_t(&input)[PDU_Size] )
        {
            response_size = ResponseBufferSize;
            Server::l2cap_input( input, PDU_Size, response, response_size, connection );
            BOOST_REQUIRE_LE( response_size, ResponseBufferSize );
        }

        void l2cap_input( const std::initializer_list< std::uint8_t >& input, typename Server::connection_data& con )
        {
            const std::vector< std::uint8_t > values( input );

            response_size = ResponseBufferSize;
            Server::l2cap_input( &values[ 0 ], values.size(), response, response_size, con );
            BOOST_REQUIRE_LE( response_size, ResponseBufferSize );
        }

        void l2cap_input( const std::initializer_list< std::uint8_t >& input )
        {
            l2cap_input( input, connection );
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

        void expected_output( const bluetoe::details::notification_data& value, const std::initializer_list< std::uint8_t >& expected, typename Server::connection_data& con )
        {
            assert( value.valid() );

            const std::vector< std::uint8_t > values( expected );
            std::uint8_t buffer[ ResponseBufferSize ];
            std::size_t  size = ResponseBufferSize;

            this->notification_output( &buffer[ 0 ], size, con, value );
            BOOST_REQUIRE_EQUAL_COLLECTIONS( values.begin(), values.end(), &buffer[ 0 ], &buffer[ size ] );
        }

        void expected_output( const bluetoe::details::notification_data& value, const std::initializer_list< std::uint8_t >& expected )
        {
            expected_output( value, expected, connection );
        }

        template < class T >
        void expected_output( const T& value, const std::initializer_list< std::uint8_t >& expected, typename Server::connection_data& con )
        {
            expected_output( find_notification_data( &value ), expected, con );
        }

        template < class T >
        void expected_output( const T& value, const std::initializer_list< std::uint8_t >& expected )
        {
            expected_output( find_notification_data( &value ), expected, connection );
        }

        static_assert( ResponseBufferSize >= 23, "min MTU size is 23, no point in using less" );

        using Server::find_notification_data;

        std::uint8_t                                response[ ResponseBufferSize ];
        std::size_t                                 response_size;
        static constexpr std::size_t                mtu_size = ResponseBufferSize;
        typename Server::connection_data            connection;
        static bluetoe::details::notification_data  notification;

    private:
        static void l2cap_layer_notify_cb( const bluetoe::details::notification_data& item )
        {
            notification = item;
        }

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
            response_size = sizeof( response );

            Server::l2cap_input( input, input_size, response, response_size, connection );

            const std::uint8_t  opcode           = response[ 0 ];
            const std::uint8_t  request_opcode   = response[ 1 ];
            const std::uint16_t attribute_handle = response[ 2 ] + ( response[ 3 ] << 8 );
            const std::uint8_t  error_code       = response[ 4 ];

            BOOST_CHECK_MESSAGE( response_size == 5, should_be_but( "PDU Size", 5, response_size ) );
            BOOST_CHECK_MESSAGE( opcode == 0x01, should_be_but( "Attribute Opcode", 0x01, opcode ) );
            BOOST_CHECK_MESSAGE( request_opcode == expected_request_opcode, should_be_but( "Request Opcode In Error", expected_request_opcode, request_opcode ) );
            BOOST_CHECK_MESSAGE( attribute_handle == expected_attribute_handle, should_be_but( "Attribute Handle In Error", expected_attribute_handle, attribute_handle ) );
            BOOST_CHECK_MESSAGE( error_code == expected_error_code, should_be_but( "Error Code", expected_error_code, error_code ) );

            return response_size == 5
                && opcode == 0x01
                && request_opcode == expected_request_opcode
                && attribute_handle == expected_attribute_handle
                && error_code == expected_error_code;
        }

    };

    template < typename Server, std::size_t ResponseBufferSize >
    bluetoe::details::notification_data request_with_reponse< Server, ResponseBufferSize >::notification;

    template < std::size_t ResponseBufferSize = 23 >
    using small_temperature_service_with_response = request_with_reponse< small_temperature_service, ResponseBufferSize >;

}

#endif
