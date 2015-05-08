#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"
#include <array>

BOOST_AUTO_TEST_SUITE( execute_write_errors )

std::uint8_t value = 0;

typedef bluetoe::server<
    bluetoe::shared_write_queue< 100 >,
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( value ), &value >
        >
    >
> value_server;

BOOST_FIXTURE_TEST_CASE( pdu_to_small, request_with_reponse< value_server > )
{
    BOOST_CHECK( check_error_response( { 0x18 }, 0x18, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( pdu_to_large, request_with_reponse< value_server > )
{
    BOOST_CHECK( check_error_response( { 0x18, 0x00, 0x01 }, 0x18, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( invalid_flags, request_with_reponse< value_server > )
{
    BOOST_CHECK( check_error_response( { 0x18, 0x02 }, 0x18, 0x0000, 0x04 ) );
    BOOST_CHECK( check_error_response( { 0x18, 0x80 }, 0x18, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( request_not_supported_if_no_buffer_exist, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x18, 0x00 }, 0x18, 0x0000, 0x06 ) );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( execute_write )

static const std::array< std::uint8_t, 16 > value_org = { {
    0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F
} };

std::array< std::uint8_t, 16 > value = value_org;

typedef bluetoe::server<
    bluetoe::shared_write_queue< 100 >,
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( value ), &value >
        >
    >
> value_server;

struct server_fixture : request_with_reponse< value_server >
{
    server_fixture()
    {
        value = value_org;
    }
};

BOOST_FIXTURE_TEST_CASE( no_queue_no_error, server_fixture )
{
    l2cap_input( { 0x18, 0x00 } );
    expected_result( { 0x19 } );
}

BOOST_FIXTURE_TEST_CASE( no_queue_no_error_chancel, server_fixture )
{
    l2cap_input( { 0x18, 0x01 } );
    expected_result( { 0x19 } );
}

struct serval_writes : server_fixture
{
    serval_writes()
    {
        l2cap_input( { 0x16, 0x03, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00 } );
        l2cap_input( { 0x16, 0x03, 0x00, 0x08, 0x00, 0x0b, 0x88, 0x99 } );
    }
};

BOOST_FIXTURE_TEST_CASE( no_change_if_canceled, serval_writes )
{
    l2cap_input( { 0x18, 0x00 } );
    expected_result( { 0x19 } );

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( value ), std::end( value ), std::begin( value_org ), std::end( value_org ) );
}

BOOST_FIXTURE_TEST_CASE( changes_applied, serval_writes )
{
    l2cap_input( { 0x18, 0x01 } );
    expected_result( { 0x19 } );

    static const std::array< std::uint8_t, 16 > writes_applied = { {
        0x02, 0x01, 0x00, 0x03,
        0x04, 0x05, 0x06, 0x07,
        0x0b, 0x88, 0x99, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F
    } };

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( value ), std::end( value ), writes_applied.begin(), writes_applied.end() );
}

BOOST_FIXTURE_TEST_CASE( no_changes_if_first_write_was_erroneous, server_fixture )
{
    // first prepare write will write over the end of the value
    l2cap_input( { 0x16, 0x03, 0x00, 0x0E, 0x00, 0x02, 0x01, 0x00 } );
    l2cap_input( { 0x16, 0x03, 0x00, 0x08, 0x00, 0x0b, 0x88, 0x99 } );

    BOOST_CHECK( check_error_response( { 0x18, 0x01 }, 0x18, 0x0003, 0x0D ) );
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( value ), std::end( value ), std::begin( value_org ), std::end( value_org ) );
}

BOOST_FIXTURE_TEST_CASE( first_change_applied_if_second_write_was_erroneous, server_fixture )
{
    // first prepare write will write over the end of the value
    l2cap_input( { 0x16, 0x03, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00 } );
    l2cap_input( { 0x16, 0x03, 0x00, 0x11, 0x00, 0x0b, 0x88, 0x99 } );

    BOOST_CHECK( check_error_response( { 0x18, 0x01 }, 0x18, 0x0003, 0x07 ) );

    static const std::array< std::uint8_t, 16 > expected_value = { {
        0x02, 0x01, 0x00, 0x03,
        0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F
    } };

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( value ), std::end( value ), std::begin( expected_value ), std::end( expected_value ) );
}

BOOST_FIXTURE_TEST_CASE( queue_is_release_after_executing, server_fixture )
{
    connection_data other_client( mtu_size );
    l2cap_input( { 0x16, 0x03, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00 }, other_client );
    expected_result( { 0x17, 0x03, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00 } );
}

BOOST_FIXTURE_TEST_CASE( queue_is_release_after_canceling, serval_writes )
{
    l2cap_input( { 0x18, 0x00 } );

    connection_data other_client( mtu_size );
    l2cap_input( { 0x16, 0x03, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00 }, other_client );
    expected_result( { 0x17, 0x03, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00 } );
}

BOOST_FIXTURE_TEST_CASE( queue_is_release_after_an_error_occured, server_fixture )
{
    // first prepare write will write over the end of the value
    l2cap_input( { 0x16, 0x03, 0x00, 0x0E, 0x00, 0x02, 0x01, 0x00 } );
    l2cap_input( { 0x18, 0x01 } );

    connection_data other_client( mtu_size );
    l2cap_input( { 0x16, 0x03, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00 }, other_client );
    expected_result( { 0x17, 0x03, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00 } );
}

BOOST_AUTO_TEST_SUITE_END()
