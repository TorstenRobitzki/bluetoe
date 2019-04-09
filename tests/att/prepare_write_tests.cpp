#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"

BOOST_AUTO_TEST_SUITE( prepare_write_errors )

BOOST_FIXTURE_TEST_CASE( no_write_buffer, test::request_with_reponse< test::three_apes_service > )
{
    BOOST_CHECK( check_error_response( { 0x16, 0x03, 0x00, 0x00, 0x00, 0xab  }, 0x16, 0x0000, 0x06 ) );
}

std::uint8_t very_large_value[ 64 ] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
};

typedef bluetoe::server<
    bluetoe::shared_write_queue< ( ( sizeof( very_large_value ) + 17 ) / 18 * 7 ) + sizeof( very_large_value ) >,
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( very_large_value ), &very_large_value >
        >
    >
> very_large_value_server;

typedef test::request_with_reponse< very_large_value_server > large_fixture;

BOOST_FIXTURE_TEST_CASE( pdu_to_small, large_fixture )
{
    BOOST_CHECK( check_error_response( { 0x16, 0x03, 0x00 }, 0x16, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( no_such_handle, large_fixture )
{
    BOOST_CHECK( check_error_response( { 0x16, 0x17, 0xAA, 0x00, 0x00, 0x12, 0x23 }, 0x16, 0xAA17, 0x0A ) );
}

BOOST_FIXTURE_TEST_CASE( invalid_handle, large_fixture )
{
    BOOST_CHECK( check_error_response( { 0x16, 0x00, 0x00, 0x00, 0x00 }, 0x16, 0x0000, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( write_protected, large_fixture )
{
    BOOST_CHECK( check_error_response( { 0x16, 0x01, 0x00, 0x00, 0x00 }, 0x16, 0x0001, 0x03 ) );
}

typedef bluetoe::server<
    bluetoe::shared_write_queue< 12 >,
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( very_large_value ), &very_large_value >
        >
    >
> large_value_server_with_small_queue;

BOOST_FIXTURE_TEST_CASE( write_queue_full, test::request_with_reponse< large_value_server_with_small_queue > )
{
    BOOST_CHECK( check_error_response(
        { 0x16, 0x03, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 },
        0x16, 0x0003, 0x09 ) );
}

BOOST_FIXTURE_TEST_CASE( write_queue_used_by_other_client, large_fixture )
{
    connection_data con1( 23 );
    connection_data con2( 23 );

    l2cap_input( { 0x16, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01 }, con1 );
    expected_result( { 0x17, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01 } );

    l2cap_input( { 0x16, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01 }, con2 );
    expected_result( { 0x01, 0x16, 0x03, 0x00, 0x09 } );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( prepare_writes )

std::uint16_t ape1 = 1, ape2 = 2, ape3 = 3;

typedef bluetoe::server<
    bluetoe::shared_write_queue< 30 >,
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< std::uint16_t, &ape1 >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAB >,
            bluetoe::bind_characteristic_value< std::uint16_t, &ape2 >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAC >,
            bluetoe::bind_characteristic_value< std::uint16_t, &ape3 >
        >
    >
> three_apes_server;

BOOST_FIXTURE_TEST_CASE( prepare_3_writes, test::request_with_reponse< three_apes_server > )
{
    l2cap_input( { 0x16, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01 } );
    expected_result( { 0x17, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01 } );

    l2cap_input( { 0x16, 0x05, 0x00, 0x00, 0x00, 0x00, 0x02 } );
    expected_result( { 0x17, 0x05, 0x00, 0x00, 0x00, 0x00, 0x02 } );

    l2cap_input( { 0x16, 0x07, 0x00, 0x00, 0x00, 0x00, 0x03 } );
    expected_result( { 0x17, 0x07, 0x00, 0x00, 0x00, 0x00, 0x03 } );
}

BOOST_FIXTURE_TEST_CASE( queue_can_be_used_after_beeing_released, test::request_with_reponse< three_apes_server > )
{
    connection_data con1( 23 );
    connection_data con2( 23 );

    l2cap_input( { 0x16, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01 }, con1 );

    l2cap_input( { 0x16, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01 }, con2 );
    expected_result( { 0x01, 0x16, 0x03, 0x00, 0x09 } );

    this->client_disconnected( con1 );

    l2cap_input( { 0x16, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01 }, con2 );
    expected_result( { 0x17, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01 } );
}

BOOST_FIXTURE_TEST_CASE( client_disconnected_can_be_called_without_queue, test::small_temperature_service_with_response<> )
{
    this->client_disconnected( connection );
}

BOOST_AUTO_TEST_SUITE_END()
