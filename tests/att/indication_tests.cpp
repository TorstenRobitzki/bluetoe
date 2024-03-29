#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/server.hpp>

#include "test_servers.hpp"
#include "attribute_io.hpp"

std::uint8_t value = 0x42;

using simple_server = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid16< 0x8C8B >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x8C8B >,
            bluetoe::bind_characteristic_value< std::uint8_t, &value >,
            bluetoe::indicate
        >
    >
>;

BOOST_AUTO_TEST_SUITE( indications_by_value )

    BOOST_FIXTURE_TEST_CASE( indication_adds_a_client_configuration, test::request_with_reponse< simple_server > )
    {
        BOOST_REQUIRE_EQUAL( int{ number_of_client_configs }, 1 );
    }

    BOOST_FIXTURE_TEST_CASE( l2cap_layer_gets_notified, test::request_with_reponse< simple_server > )
    {
        indicate( value );

        BOOST_CHECK( notification.valid() );
        BOOST_CHECK_EQUAL( notification.attribute_table_index(), 2u );
        BOOST_CHECK_EQUAL( notification_type, bluetoe::details::notification_type::indication );
    }

    BOOST_FIXTURE_TEST_CASE( if_client_configuration_is_not_enabled_not_output, test::request_with_reponse< simple_server > )
    {
        channel_data_t< bluetoe::details::link_state_no_security > client_configuration;
        client_configuration.queue_indication( 0 );

        std::size_t size = client_configuration.negotiated_mtu();
        l2cap_output( begin(), size, client_configuration );

        BOOST_CHECK_EQUAL( size, 0u );
    }

    BOOST_FIXTURE_TEST_CASE( output_if_enables, test::request_with_reponse< simple_server > )
    {
        channel_data_t< bluetoe::details::link_state > client_configuration;
        client_configuration.queue_indication( 0 );
        client_configuration.client_configurations().flags( 0, 2 ); // 2 == indications

        std::size_t size = client_configuration.negotiated_mtu();
        l2cap_output( begin(), size, client_configuration );

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

    BOOST_FIXTURE_TEST_CASE( notify_first_16bit_uuid, test::request_with_reponse< server > )
    {
        indicate< bluetoe::characteristic_uuid16< 0x2222 > >();

        BOOST_REQUIRE( notification.valid() );
        BOOST_CHECK_EQUAL( notification.attribute_table_index(), 7u );
        BOOST_CHECK_EQUAL( notification.client_characteristic_configuration_index(), 1u );
        BOOST_CHECK_EQUAL( notification_type, bluetoe::details::notification_type::indication );
    }

    BOOST_FIXTURE_TEST_CASE( notify_second_16bit_uuid, test::request_with_reponse< server > )
    {
        indicate< bluetoe::characteristic_uuid16< 0x3333 > >();

        BOOST_REQUIRE( notification.valid() );
        BOOST_CHECK_EQUAL( notification.attribute_table_index(), 13u );
        BOOST_CHECK_EQUAL( notification.client_characteristic_configuration_index(), 2u );
        BOOST_CHECK_EQUAL( notification_type, bluetoe::details::notification_type::indication );
    }

    // This test should not compile
    /*
    BOOST_FIXTURE_TEST_CASE( indicating_not_configured_will_result_in_compiletime_error, test::request_with_reponse< server > )
    {
        indicate< bluetoe::characteristic_uuid16< 0x1111 > >();
    }
    */

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( confirmation )

    BOOST_FIXTURE_TEST_CASE( confirmation_is_forwared_to_the_link_layer, test::request_with_reponse< simple_server > )
    {
        l2cap_input( { 0x1E } );

        BOOST_CHECK( !notification.valid() );
        BOOST_CHECK_EQUAL( notification_type, bluetoe::details::notification_type::confirmation );
    }

    BOOST_FIXTURE_TEST_CASE( no_response_to_a_confirmation, test::request_with_reponse< simple_server > )
    {
        l2cap_input( { 0x1E } );

        BOOST_CHECK_EQUAL( response_size, 0u );
    }

    BOOST_FIXTURE_TEST_CASE( broken_pdu, test::request_with_reponse< simple_server > )
    {
        check_error_response(
            { 0x1E, 0x00 },
            0x1E, 0x0000, 0x04 );
    }

BOOST_AUTO_TEST_SUITE_END()

using server_with_notify_on_subscription = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid16< 0x1234 >,

        // Handle of CCCD is: 0x0004
        bluetoe::characteristic<
            bluetoe::bind_characteristic_value< std::uint8_t, &value >,
            bluetoe::characteristic_uuid16< 0x0001 >,
            bluetoe::notify,
            bluetoe::notify_on_subscription
        >,

        // Handle of CCCD is: 0x2000
        bluetoe::characteristic<
            bluetoe::bind_characteristic_value< std::uint8_t, &value >,
            bluetoe::attribute_handles< 0x1000, 0x2000, 0x3000 >,
            bluetoe::characteristic_uuid16< 0x0002 >,
            bluetoe::indicate,
            bluetoe::indicate_on_subscription
        >
    >
>;

BOOST_AUTO_TEST_SUITE( notify_on_subscription )

    BOOST_FIXTURE_TEST_CASE( test_notification, test::request_with_reponse< server_with_notify_on_subscription > )
    {
        l2cap_input( { 0x12, 0x04, 0x00, 0x01, 0x00 } );

        BOOST_CHECK_EQUAL( connection.client_configurations().flags( 0 ), 0x0001 );

        BOOST_REQUIRE( notification.valid() );
        BOOST_CHECK_EQUAL( notification.attribute_table_index(), 2u );
        BOOST_CHECK( notification_type == bluetoe::details::notification_type::notification );
    }

    BOOST_FIXTURE_TEST_CASE( test_indication, test::request_with_reponse< server_with_notify_on_subscription > )
    {
        l2cap_input( { 0x12, 0x00, 0x30, 0x02, 0x00 } );

        BOOST_CHECK_EQUAL( connection.client_configurations().flags( 1 ), 0x0002 );

        BOOST_REQUIRE( notification.valid() );
        BOOST_CHECK_EQUAL( notification.attribute_table_index(), 5u );
        BOOST_CHECK( notification_type == bluetoe::details::notification_type::indication );
    }

    BOOST_FIXTURE_TEST_CASE( no_cb_called_when_not_configured, test::request_with_reponse< simple_server > )
    {
        l2cap_input( { 0x12, 0x04, 0x00, 0x01, 0x00 } );

        BOOST_CHECK_EQUAL( connection.client_configurations().flags( 0 ), 0x0001 );
        BOOST_CHECK( !notification.valid() );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( access_error )

    std::uint8_t error( std::size_t, std::uint8_t*, std::size_t& )
    {
        // something != bluetoe::error_codes::success;
        return bluetoe::error_codes::invalid_pdu;
    }

    using server = bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid16< 0x1234 >,
            bluetoe::characteristic<
                bluetoe::free_read_handler< &error >,
                bluetoe::characteristic_uuid16< 0x0001 >,
                bluetoe::indicate
            >
        >
    >;

    BOOST_FIXTURE_TEST_CASE( read_error, test::request_with_reponse< server > )
    {
        l2cap_input( { 0x12, 0x04, 0x00, 0x02, 0x00 } );

        std::uint8_t output_buffer[ 50 ];
        std::size_t  output_size = sizeof( output_buffer );

        l2cap_output( output_buffer, output_size, connection );

        BOOST_CHECK_EQUAL( output_size, 0u );
    }

BOOST_AUTO_TEST_SUITE_END()
