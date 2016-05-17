#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/server.hpp>
#include "../test_servers.hpp"

std::uint8_t value = 0x42;

typedef bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid16< 0x8C8B >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x8C8B >,
            bluetoe::bind_characteristic_value< std::uint8_t, &value >,
            bluetoe::indicate
        >
    >
> simple_server;

BOOST_AUTO_TEST_SUITE( indications_by_value )

    BOOST_FIXTURE_TEST_CASE( indication_adds_a_client_configuration, request_with_reponse< simple_server > )
    {
        BOOST_REQUIRE_EQUAL( int{ number_of_client_configs }, 1u );
    }

    BOOST_FIXTURE_TEST_CASE( l2cap_layer_gets_notified, request_with_reponse< simple_server > )
    {
        indicate( value );

        BOOST_CHECK( notification.valid() );
        BOOST_CHECK_EQUAL( notification.handle(), 3 );
        BOOST_CHECK_EQUAL( notification_type, simple_server::indication );
    }

    BOOST_FIXTURE_TEST_CASE( if_client_configuration_is_not_enabled_not_output, request_with_reponse< simple_server > )
    {
        connection_data client_configuration( mtu_size );

        std::size_t size = mtu_size;
        indication_output( begin(), size, client_configuration, 0 );

        BOOST_CHECK_EQUAL( size, 0 );
    }

    BOOST_FIXTURE_TEST_CASE( output_if_enables, request_with_reponse< simple_server > )
    {
        connection_data client_configuration( mtu_size );
        client_configuration.client_configurations().flags( 0, 2 ); // 2 == indications

        std::size_t size = mtu_size;
        indication_output( begin(), size, client_configuration, 0 );

        static const std::uint8_t expected_output[] = { 0x1D, 0x03, 0x00, 0x42 };
        BOOST_CHECK_EQUAL( size, sizeof( expected_output ) );
        BOOST_CHECK_EQUAL_COLLECTIONS( begin(), begin() + size, std::begin( expected_output ), std::end( expected_output ) );
    }


BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( indications_by_uuid )

    static const std::uint16_t value1 = 0x1111;
    static const std::uint16_t value2 = 0x2222;
    static const std::uint16_t value3 = 0x3333;
    static const std::uint16_t dummy  = 0x4444;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8B >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x1111 >,
                bluetoe::bind_characteristic_value< decltype( value1 ), &value1 >,
                bluetoe::notify
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0xffff >,
                bluetoe::bind_characteristic_value< decltype( dummy ), &dummy >
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x2222 >,
                bluetoe::bind_characteristic_value< decltype( value2 ), &value2 >,
                bluetoe::notify,
                bluetoe::indicate
            >
        >,
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8C >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0xffff >,
                bluetoe::bind_characteristic_value< decltype( dummy ), &dummy >
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x3333 >,
                bluetoe::bind_characteristic_value< decltype( value3 ), &value3 >,
                bluetoe::indicate
            >
        >
    > server;

    BOOST_FIXTURE_TEST_CASE( notify_first_16bit_uuid, request_with_reponse< server > )
    {
        indicate< bluetoe::characteristic_uuid16< 0x2222 > >();

        BOOST_REQUIRE( notification.valid() );
        BOOST_CHECK_EQUAL( notification.handle(), 8 );
        BOOST_CHECK_EQUAL( notification.client_characteristic_configuration_index(), 1 );
        BOOST_CHECK_EQUAL( notification_type, server::indication );
    }

    BOOST_FIXTURE_TEST_CASE( notify_second_16bit_uuid, request_with_reponse< server > )
    {
        indicate< bluetoe::characteristic_uuid16< 0x3333 > >();

        BOOST_REQUIRE( notification.valid() );
        BOOST_CHECK_EQUAL( notification.handle(), 14 );
        BOOST_CHECK_EQUAL( notification.client_characteristic_configuration_index(), 2 );
        BOOST_CHECK_EQUAL( notification_type, server::indication );
    }

    // This test should not compile
    /*
    BOOST_FIXTURE_TEST_CASE( indicating_not_configured_will_result_in_compiletime_error, request_with_reponse< server > )
    {
        indicate< bluetoe::characteristic_uuid16< 0x1111 > >();
    }
    */

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( confirmation )

    BOOST_FIXTURE_TEST_CASE( confirmation_is_forwared_to_the_link_layer, request_with_reponse< simple_server > )
    {
        l2cap_input( { 0x1E } );

        BOOST_CHECK( !notification.valid() );
        BOOST_CHECK_EQUAL( notification_type, simple_server::confirmation );
    }

    BOOST_FIXTURE_TEST_CASE( no_response_to_a_confirmation, request_with_reponse< simple_server > )
    {
        l2cap_input( { 0x1E } );

        BOOST_CHECK_EQUAL( response_size, 0 );
    }

    BOOST_FIXTURE_TEST_CASE( broken_pdu, request_with_reponse< simple_server > )
    {
        check_error_response(
            { 0x1E, 0x00 },
            0x1E, 0x0000, 0x04 );
    }

BOOST_AUTO_TEST_SUITE_END()
