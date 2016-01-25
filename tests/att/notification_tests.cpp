#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"
#include <array>

BOOST_AUTO_TEST_SUITE( notifications_by_value )

    std::uint8_t value = 0;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8B >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8B >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value >,
                bluetoe::notify
            >
        >
    > simple_server;

    BOOST_FIXTURE_TEST_CASE( l2cap_layer_gets_notified, request_with_reponse< simple_server > )
    {
        notify( value );

        BOOST_CHECK( notification.valid() );
        BOOST_CHECK_EQUAL( notification.value_attribute().uuid, 0x8C8B );
        BOOST_CHECK_EQUAL( notification_type, simple_server::notification );
    }

    BOOST_FIXTURE_TEST_CASE( no_output_when_notification_not_enabled, request_with_reponse< simple_server > )
    {
        expected_output( value, {} );
    }

    BOOST_FIXTURE_TEST_CASE( notification_if_enabled, request_with_reponse< simple_server > )
    {
        l2cap_input( { 0x12, 0x04, 0x00, 0x01, 0x00 } );
        expected_result( { 0x13 } );

        value = 0xab;
        expected_output( value, { 0x1B, 0x03, 0x00, 0xab } );
    }

    std::uint8_t large_buffer[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29
    };

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8B >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8B >,
                bluetoe::bind_characteristic_value< decltype( large_buffer ), &large_buffer >,
                bluetoe::notify
            >
        >
    > large_value_notify_server;

    BOOST_FIXTURE_TEST_CASE( notification_data_is_clipped_to_mtu_size_23, request_with_reponse< large_value_notify_server > )
    {
        l2cap_input( { 0x12, 0x04, 0x00, 0x01, 0x00 } );
        expected_result( { 0x13 } );

        expected_output( large_buffer, {
            0x1B, 0x03, 0x00,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19
        } );
    }

    std::uint8_t value_a1 = 1;
    std::uint8_t value_a2 = 2;
    std::uint8_t value_b1 = 3;
    std::uint8_t value_c1 = 4;
    std::uint8_t value_c2 = 5;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8B >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8B >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value_a1 >,
                bluetoe::notify
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8C >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value_a2 >,
                bluetoe::notify
            >
        >,
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8C >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8D >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value_b1 >,
                bluetoe::notify
            >
        >,
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8D >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8E >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value_c1 >,
                bluetoe::notify
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8F >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value_c2 >,
                bluetoe::notify
            >
        >
    > server_with_multiple_char;

    BOOST_FIXTURE_TEST_CASE( all_values_notifable, request_with_reponse< server_with_multiple_char > )
    {
        notify( value_a1 );
        notify( value_a2 );
        notify( value_b1 );
        notify( value_c1 );
        notify( value_c2 );
    }

    BOOST_FIXTURE_TEST_CASE( notify_c1, request_with_reponse< server_with_multiple_char > )
    {
        l2cap_input( { 0x12, 0x0F, 0x00, 0x01, 0x00 } );
        expected_result( { 0x13 } );

        expected_output( value_c1, { 0x1B, 0x0E, 0x00, 0x04 } );
    }

    BOOST_FIXTURE_TEST_CASE( notify_b1_on_two_connections, request_with_reponse< server_with_multiple_char > )
    {
        connection_data con1( 23 ), con2( 23 );

        // enable notification on connection 1
        l2cap_input( { 0x12, 0x0B, 0x00, 0x01, 0x00 }, con1 );
        expected_result( { 0x13 } );

        //  expect output just on connection 1
        expected_output( value_b1, { 0x1B, 0x0A, 0x00, 0x03 }, con1 );
        expected_output( value_b1, {}, con2 );

    }
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( notifications_by_uuid )

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
                bluetoe::indicate,
                bluetoe::notify
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
        notify< bluetoe::characteristic_uuid16< 0x1111 > >();

        BOOST_REQUIRE( notification.valid() );
        BOOST_CHECK_EQUAL( notification.value_attribute().uuid, 0x1111 );
        BOOST_CHECK_EQUAL( notification.client_characteristic_configuration_index(), 0 );
        BOOST_CHECK_EQUAL( notification_type, server::notification );
    }

    BOOST_FIXTURE_TEST_CASE( notify_second_16bit_uuid, request_with_reponse< server > )
    {
        notify< bluetoe::characteristic_uuid16< 0x2222 > >();

        BOOST_REQUIRE( notification.valid() );
        BOOST_CHECK_EQUAL( notification.value_attribute().uuid, 0x2222 );
        BOOST_CHECK_EQUAL( notification.client_characteristic_configuration_index(), 1 );
        BOOST_CHECK_EQUAL( notification_type, server::notification );
    }

    // This test should not compile
    /*
    BOOST_FIXTURE_TEST_CASE( notify_third_16bit_uuid, request_with_reponse< server > )
    {
        notify< bluetoe::characteristic_uuid16< 0x3333 > >();
    }
    */
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( access_client_characteristic_configuration )

    std::uint8_t value1 = 0, value2;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8B >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8B >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value1 >,
                bluetoe::notify
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8C >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value2 >,
                bluetoe::notify
            >
        >
    > simple_server;

    BOOST_FIXTURE_TEST_CASE( read_default_values, request_with_reponse< simple_server > )
    {
        l2cap_input( { 0x0A, 0x04, 0x00 } );
        expected_result( { 0x0B, 0x00, 0x00 } );

        l2cap_input( { 0x0A, 0x07, 0x00 } );
        expected_result( { 0x0B, 0x00, 0x00 } );
    }

    BOOST_FIXTURE_TEST_CASE( read_blob_with_offset_1, request_with_reponse< simple_server > )
    {
        l2cap_input( { 0x0C, 0x04, 0x00, 0x01, 0x00 } );
        expected_result( { 0x0D, 0x00 } );
    }

    BOOST_FIXTURE_TEST_CASE( set_and_read, request_with_reponse< simple_server > )
    {
        l2cap_input( { 0x12, 0x04, 0x00, 0x01, 0x00 } );
        expected_result( { 0x13 } );

        l2cap_input( { 0x12, 0x07, 0x00, 0x00, 0x00 } );
        expected_result( { 0x13 } );

        // read by type
        l2cap_input( { 0x08, 0x01, 0x00, 0xFF, 0xFF, 0x02, 0x29 } );
        expected_result( {
            0x09, 0x04,              // opcode and size
            0x04, 0x00, 0x01, 0x00,  // handle and data
            0x07, 0x00, 0x00, 0x00   // handle and data
        } );
    }

BOOST_AUTO_TEST_SUITE_END()
