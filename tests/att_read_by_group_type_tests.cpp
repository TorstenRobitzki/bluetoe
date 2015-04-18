#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"

BOOST_AUTO_TEST_SUITE( read_by_group_type_errors )

BOOST_FIXTURE_TEST_CASE( start_handle_zero, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x10, 0x00, 0x00, 0xff, 0xff, 0x00, 0x28 }, 0x10, 0x0000, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( start_handle_greater_than_end_handle, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x10, 0x05, 0x00, 0x04, 0x00, 0x00, 0x28 }, 0x10, 0x0005, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( invalid_request_size, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x10, 0x05, 0x00, 0x05, 0x00 }, 0x10, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( handle_out_of_range, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x10, 0x40, 0x00, 0xff, 0xff, 0x00, 0x28 }, 0x10, 0x0040, 0x0A ) );
}

BOOST_FIXTURE_TEST_CASE( unsupprorted_group_type, small_temperature_service_with_response<> )
{
    // every type but the «Primary Service» is invalid for GATT
    BOOST_CHECK( check_error_response( { 0x10, 0x01, 0x00, 0xff, 0xff, 0x01, 0x28 }, 0x10, 0x0001, 0x10 ) );
}

BOOST_FIXTURE_TEST_CASE( unsupprorted_group_type_128bit, small_temperature_service_with_response<> )
{
    // every type but the «Primary Service» is invalid for GATT
    BOOST_CHECK( check_error_response(
        {
            0x10, 0x01, 0x00, 0xff, 0xff,
            0x00, 0x01, 0x02, 0x03,
            0x04, 0x05, 0x06, 0x06,
            0x08, 0x09, 0x0a, 0x0b,
            0x0c, 0x0d, 0x0e, 0x0f
        }, 0x10, 0x0001, 0x10 ) );
}

BOOST_AUTO_TEST_SUITE_END()