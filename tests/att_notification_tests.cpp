#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"
#include <array>

BOOST_AUTO_TEST_SUITE( simple_notify )

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

        BOOST_CHECK_EQUAL( &value, notification );
    }

    BOOST_FIXTURE_TEST_CASE( no_output_when_notification_not_enabled, request_with_reponse< simple_server > )
    {
        expected_output( value, {} );
    }

    BOOST_FIXTURE_TEST_CASE( notification_if_enabled, request_with_reponse< simple_server > )
    {
        // l2cap_input( { 0x12, 0x04, 0x00, 0x01, 0x00 } );
        // expected_result( { 0x13 } );

        // value = 0xab;
        // expected_output( value, { 0x1B, 0x03, 0x00, 0xab } );
    }

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
