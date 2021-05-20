#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <test_servers.hpp>
#include <test_services.hpp>

BOOST_AUTO_TEST_SUITE( read_by_group_type_errors )

BOOST_FIXTURE_TEST_CASE( start_handle_zero, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x10, 0x00, 0x00, 0xff, 0xff, 0x00, 0x28 }, 0x10, 0x0000, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( start_handle_greater_than_end_handle, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x10, 0x05, 0x00, 0x04, 0x00, 0x00, 0x28 }, 0x10, 0x0005, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( invalid_request_size, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x10, 0x05, 0x00, 0x05, 0x00 }, 0x10, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( handle_out_of_range, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x10, 0x40, 0x00, 0xff, 0xff, 0x00, 0x28 }, 0x10, 0x0040, 0x0A ) );
}

BOOST_FIXTURE_TEST_CASE( unsupprorted_group_type, test::small_temperature_service_with_response<> )
{
    // every type but the «Primary Service» is invalid for GATT
    BOOST_CHECK( check_error_response( { 0x10, 0x01, 0x00, 0xff, 0xff, 0x01, 0x28 }, 0x10, 0x0001, 0x10 ) );
}

BOOST_FIXTURE_TEST_CASE( unsupprorted_group_type_128bit, test::small_temperature_service_with_response<> )
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

BOOST_FIXTURE_TEST_CASE( request_out_of_range, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response(
        { 0x10, 0x02, 0x00, 0xff, 0xff, 0x00, 0x28 }, 0x10, 0x0002, 0x0a ) );

}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( discover_primary_services )

BOOST_FIXTURE_TEST_CASE( single_service, test::small_temperature_service_with_response<> )
{
    l2cap_input( { 0x10, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28 } );

    static const std::uint8_t expected_result[] = {
        0x11, 0x14,                 // response code, size
        0x01, 0x00, 0x03, 0x00,     // 0x0001 - 0x0003
        0xA9, 0x3C, 0xC7, 0x5B,     // 128 bit UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result ), std::end( expected_result ) );
}

typedef bluetoe::server<
    test::global_temperature_service,
    test::service_with_3_characteristics,
    test::cycling_speed_and_cadence_service
> server_with_3_characteristics;

typedef test::request_with_reponse< server_with_3_characteristics, 100 > request_with_reponse_server_with_3_characteristics_100;

BOOST_FIXTURE_TEST_CASE( different_attribute_data_size, request_with_reponse_server_with_3_characteristics_100 )
{
    l2cap_input( { 0x10, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28 } );

    static const std::uint8_t expected_result[] = {
        0x11, 0x14,                 // response code, size
        0x01, 0x00, 0x03, 0x00,     // handle 0x0001 - 0x0003
        0x2A, 0xD9, 0x91, 0x11,     // service_uuid global_temperature_service
        0xAB, 0x5B, 0x58, 0xB0,
        0x3B, 0x4F, 0x50, 0x44,
        0x52, 0x6E, 0x42, 0xF0,
        0x04, 0x00, 0x0A, 0x00,     // handle 0x0004 - 0x000A
        0xA9, 0x3C, 0xC7, 0x5B,
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result ), std::end( expected_result ) );
}

typedef test::request_with_reponse< server_with_3_characteristics, 41 > request_with_reponse_server_with_3_characteristics_41;

BOOST_FIXTURE_TEST_CASE( output_to_small_for_more_than_one_service, request_with_reponse_server_with_3_characteristics_41 )
{
    l2cap_input( { 0x10, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28 } );

    static const std::uint8_t expected_result[] = {
        0x11, 0x14,                 // response code, size
        0x01, 0x00, 0x03, 0x00,     // handle 0x0001 - 0x0003
        0x2A, 0xD9, 0x91, 0x11,     // service_uuid global_temperature_service
        0xAB, 0x5B, 0x58, 0xB0,
        0x3B, 0x4F, 0x50, 0x44,
        0x52, 0x6E, 0x42, 0xF0
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result ), std::end( expected_result ) );
}

typedef bluetoe::server<
    test::global_temperature_service,
    test::cycling_speed_and_cadence_service,
    test::service_with_3_characteristics,
    bluetoe::no_gap_service_for_gatt_servers
> server_with_16bit_characteristics_in_the_middle;

typedef test::request_with_reponse< server_with_16bit_characteristics_in_the_middle, 100 > server_with_16bit_characteristics_in_the_middle_100;

/**
 * #53 Read by Group Type Request should not return lists that contain gaps
 *
 * In order for the GATT Discover All Primary Services procedure to work correctly,
 * Read by Group Type Request should not return lists that contains gaps.
 * Group Type Request is used to discover services, and can return only services of
 * the same UUID length. If the server contains mixed 16-bit and 128-bit UUIDs,
 * the server shall not assemble lists where two services are contain,
 * where a service of the other kind would be in the middle.
 *
 * For example, a server containing a 16-bit UUID service, followed by a 128-bit UUID service,
 * followed by a 16-bit UUID service again, the server should not put both 16-bit UUID
 * services into one Read by Group Type Response.
 */

BOOST_FIXTURE_TEST_CASE( different_attribute_data_size_without_gap, server_with_16bit_characteristics_in_the_middle_100 )
{
    l2cap_input( { 0x10, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28 } );

    static const std::uint8_t expected_result_1[] = {
        0x11, 0x14,                 // response code, size
        0x01, 0x00, 0x03, 0x00,     // handle 0x0001 - 0x0003
        0x2A, 0xD9, 0x91, 0x11,     // service_uuid global_temperature_service
        0xAB, 0x5B, 0x58, 0xB0,
        0x3B, 0x4F, 0x50, 0x44,
        0x52, 0x6E, 0x42, 0xF0,
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result_1 ), std::end( expected_result_1 ) );

    l2cap_input( { 0x10, 0x04, 0x00, 0xff, 0xff, 0x00, 0x28 } );

    static const std::uint8_t expected_result_2[] = {
        0x11, 0x6,                  // response code, size
        0x04, 0x00, 0x08, 0x00,     // handle 0x0004 - 0x000A
        0x16, 0x18                  // Cycling Speed and Cadence
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result_2 ), std::end( expected_result_2 ) );

    l2cap_input( { 0x10, 0x09, 0x00, 0xff, 0xff, 0x00, 0x28 } );

    static const std::uint8_t expected_result_3[] = {
        0x11, 0x14,                 // response code, size
        0x09, 0x00, 0x0F, 0x00,     // handle 0x0001 - 0x0003
        0xA9, 0x3C, 0xC7, 0x5B,     // 128 bit UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result_3 ), std::end( expected_result_3 ) );

    l2cap_input( { 0x10, 0x10, 0x00, 0xff, 0xff, 0x00, 0x28 } );

    static const std::uint8_t expected_result_4[] = {
        0x01,                       // Error Response
        0x10,                       // request opcode
        0x10, 0x00,                 // request handle
        0x0A                        // «Attribute Not Found»
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result_4 ), std::end( expected_result_4 ) );
}

BOOST_FIXTURE_TEST_CASE( read_16bit_uuid_in_gap, test::request_with_reponse< server_with_16bit_characteristics_in_the_middle > )
{
    l2cap_input( { 0x10, 0x04, 0x00, 0xff, 0xff, 0x00, 0x28 } );

    static const std::uint8_t expected_result[] = {
        0x11, 0x06,                 // response code, size
        0x04, 0x00, 0x08, 0x00,     // handle 0x0001 - 0x0003
        0x16, 0x18                  // service_uuid cycling_speed_and_cadence_service
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result ), std::end( expected_result ) );
}

typedef bluetoe::server<
    test::cycling_speed_and_cadence_service,
    test::cycling_speed_and_cadence_service,
    test::cycling_speed_and_cadence_service,
    test::cycling_speed_and_cadence_service
> server_with_4_bicycles;

BOOST_FIXTURE_TEST_CASE( output_to_small_to_read_all_services, test::request_with_reponse< server_with_4_bicycles > )
{
    l2cap_input( { 0x10, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28 } );

    static const std::uint8_t expected_result[] = {
        0x11, 0x06,                 // response code, size
        0x01, 0x00, 0x05, 0x00,
        0x16, 0x18,
        0x06, 0x00, 0x0A, 0x00,
        0x16, 0x18,
        0x0B, 0x00, 0x0F, 0x00,
        0x16, 0x18
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result ), std::end( expected_result ) );
}

using server_with_fixed_handles = bluetoe::server<
    bluetoe::service<
        bluetoe::attribute_handle< 0x0100 >,
        bluetoe::service_uuid16< 0x1801 >,
        bluetoe::characteristic<
            bluetoe::attribute_handles< 0x1000, 0x2000, 0x3000 >,
            bluetoe::characteristic_uuid16< 0x2A05 >,
            bluetoe::indicate,
            bluetoe::fixed_uint32_value< 0xFFFF0001 >
        >
    >,
    test::cycling_speed_and_cadence_service,
    bluetoe::no_gap_service_for_gatt_servers
>;

BOOST_FIXTURE_TEST_CASE( discover_services_with_fixed_handles, test::request_with_reponse< server_with_fixed_handles > )
{
    l2cap_input( { 0x10, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28 } );

    static const std::uint8_t expected_result[] = {
        0x11, 0x06,                 // response code, size
        0x00, 0x01, 0x00, 0x30,
        0x01, 0x18,
        0x01, 0x30, 0x05, 0x30,
        0x16, 0x18
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result ), std::end( expected_result ) );
}

BOOST_AUTO_TEST_SUITE_END()
