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

BOOST_AUTO_TEST_SUITE( read_by_type )

BOOST_FIXTURE_TEST_CASE( read_a_single_attribute, small_temperature_service_with_response<> )
{
    l2cap_input( { 0x08, 0x01, 0x00, 0xff, 0xff, 0x03, 0x28 } );

    static const std::uint8_t expected_result[] = {
        0x09, 0x15,                 // response code, size = 2 for handle and 19 for attribute value (Properties, Value Handle + UUID)
        0x02, 0x00,                 // attribute handle
        0x02,                       // Characteristic Properties (read)
        0x03, 0x00,                 // Characteristic Value Handle
        0xAA, 0x3C, 0xC7, 0x5B,     // Characteristic UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result ), std::end( expected_result ) );
}

BOOST_FIXTURE_TEST_CASE( read_a_single_attribute_using_128bit_uuid, small_temperature_service_with_response<> )
{
    l2cap_input( {
        0x08, 0x01, 0x00, 0xff, 0xff,
        0xFB, 0x34, 0x9B, 0x5F,     // 16 bit UUID extended to 128 bit
        0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00,
        0x03, 0x28, 0x00, 0x00 } );

    static const std::uint8_t expected_result[] = {
        0x09, 0x15,                 // response code, size = 2 for handle and 19 for attribute value (Properties, Value Handle + UUID)
        0x02, 0x00,                 // attribute handle
        0x02,                       // Characteristic Properties (read)
        0x03, 0x00,                 // Characteristic Value Handle
        0xAA, 0x3C, 0xC7, 0x5B,     // Characteristic UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result ), std::end( expected_result ) );
}

BOOST_AUTO_TEST_SUITE_END()
