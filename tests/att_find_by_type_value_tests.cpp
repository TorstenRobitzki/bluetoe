#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"
#include "test_services.hpp"

BOOST_AUTO_TEST_SUITE( find_by_type_value_errors )

BOOST_FIXTURE_TEST_CASE( start_handle_zero, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x06, 0x00, 0x00, 0xff, 0xff, 0x00, 0x28, 0x16, 0x18 }, 0x06, 0x0000, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( start_handle_greater_than_end_handle, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x06, 0x05, 0x00, 0x04, 0x00, 0x00, 0x28, 0x16, 0x18 }, 0x06, 0x0005, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( invalid_request_size, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x06, 0x05, 0x00, 0x04, 0x00, 0x00, 0x28, 0x16 }, 0x06, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( handle_out_of_range, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x06, 0x40, 0x10, 0xff, 0xff, 0x00, 0x28, 0x16, 0x18 }, 0x06, 0x1040, 0x0A ) );
}

BOOST_FIXTURE_TEST_CASE( unsupprorted_group_type, small_temperature_service_with_response<> )
{
    // every type but the «Primary Service» is invalid for GATT
    BOOST_CHECK( check_error_response( { 0x06, 0x01, 0x00, 0xff, 0xff, 0x01, 0x28, 0x16, 0x18 }, 0x06, 0x0001, 0x10 ) );
}

BOOST_AUTO_TEST_SUITE_END()
