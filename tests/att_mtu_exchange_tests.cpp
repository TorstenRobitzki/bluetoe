
#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"

BOOST_FIXTURE_TEST_CASE( pdu_to_small, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x02, 0x50 }, 0x02, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( pdu_to_large, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x02, 0x50, 0x00, 0x01 }, 0x02, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( mtu_to_small, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x02, 0x16, 0x00 }, 0x02, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( mtu_equal_default_mtu, small_temperature_service_with_response<> )
{
    // l2cap_input();
    // expected_result();
}

BOOST_FIXTURE_TEST_CASE( valid_mtu, small_temperature_service_with_response<> )
{

}