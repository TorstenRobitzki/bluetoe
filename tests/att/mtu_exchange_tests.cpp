
#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"

BOOST_FIXTURE_TEST_CASE( pdu_to_small, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x02, 0x50 }, 0x02, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( pdu_to_large, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x02, 0x50, 0x00, 0x01 }, 0x02, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( mtu_to_small, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x02, 0x16, 0x00 }, 0x02, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( mtu_equal_default_mtu, test::small_temperature_service_with_response<> )
{
    l2cap_input( { 0x02, 0x17, 0x00 } );
    expected_result( { 0x03, 0x17, 0x00 } );
    BOOST_CHECK_EQUAL( connection.negotiated_mtu(), 0x0017 );
}

BOOST_FIXTURE_TEST_CASE( client_mtu_is_smaller, test::small_temperature_service_with_response< 0x100 > )
{
    l2cap_input( { 0x02, 0x22, 0x00 } );
    expected_result( { 0x03, 0x00, 0x01 } );
    BOOST_CHECK_EQUAL( connection.negotiated_mtu(), 0x0022 );
}

BOOST_FIXTURE_TEST_CASE( client_mtu_is_larger, test::small_temperature_service_with_response< 0x100 > )
{
    l2cap_input( { 0x02, 0x11, 0x11 } );
    expected_result( { 0x03, 0x00, 0x01 } );
    BOOST_CHECK_EQUAL( connection.negotiated_mtu(), 0x100 );
}

char large_data[ 30 ];

typedef bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( large_data ), &large_data >
        >
    >
> large_data_service;

typedef test::request_with_reponse< large_data_service, 30 > request_with_reponse_large_data_service_30;

BOOST_FIXTURE_TEST_CASE( server_honors_negotiated_mtu, request_with_reponse_large_data_service_30 )
{
    l2cap_input( { 0x02, 0x1D, 0x00 } );
    BOOST_CHECK_EQUAL( connection.negotiated_mtu(), 29 );

    // now read the characteristic value attribute
    l2cap_input( { 0x0A, 0x03, 0x00 } );
    BOOST_CHECK_EQUAL( response_size, 29 );
}
