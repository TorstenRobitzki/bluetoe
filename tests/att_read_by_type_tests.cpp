#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"
#include "test_services.hpp"

BOOST_AUTO_TEST_SUITE( read_by_type_errors )

BOOST_FIXTURE_TEST_CASE( invalid_request_size, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x08, 0x01, 0x00, 0xff, 0xff, 0x03 }, 0x08, 0x0000, 0x04 ) );
    BOOST_CHECK( check_error_response( { 0x08, 0x01, 0x00, 0xff, 0xff, 0x03, 0x04, 0x05, 0x06 }, 0x08, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( start_handle_zero, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x08, 0x00, 0x00, 0xff, 0xff, 0x03, 0x28 }, 0x08, 0x0000, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( start_handle_greater_than_end_handle, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x08, 0x05, 0x00, 0x04, 0x00, 0x03, 0x28 }, 0x08, 0x0005, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( handle_out_of_range, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x08, 0x40, 0x00, 0xff, 0xff, 0x00, 0x28 }, 0x08, 0x0040, 0x0A ) );
}

BOOST_AUTO_TEST_SUITE_END()
