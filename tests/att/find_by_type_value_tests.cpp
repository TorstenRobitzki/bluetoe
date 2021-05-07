#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"
#include "test_services.hpp"

BOOST_AUTO_TEST_SUITE( find_by_type_value_errors )

BOOST_FIXTURE_TEST_CASE( start_handle_zero, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x06, 0x00, 0x00, 0xff, 0xff, 0x00, 0x28, 0x16, 0x18 }, 0x06, 0x0000, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( start_handle_greater_than_end_handle, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x06, 0x05, 0x00, 0x04, 0x00, 0x00, 0x28, 0x16, 0x18 }, 0x06, 0x0005, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( invalid_request_size, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x06, 0x05, 0x00, 0x04, 0x00, 0x00, 0x28, 0x16 }, 0x06, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( handle_out_of_range, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x06, 0x40, 0x10, 0xff, 0xff, 0x00, 0x28, 0x16, 0x18 }, 0x06, 0x1040, 0x0A ) );
}

BOOST_FIXTURE_TEST_CASE( unsupprorted_group_type, test::small_temperature_service_with_response<> )
{
    // every type but the «Primary Service» is invalid for GATT
    BOOST_CHECK( check_error_response( { 0x06, 0x01, 0x00, 0xff, 0xff, 0x01, 0x28, 0x16, 0x18 }, 0x06, 0x0001, 0x10 ) );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( find_by_type_value )

BOOST_FIXTURE_TEST_CASE( discover_primary_service_by_service_uuid, test::request_with_reponse< test::three_apes_service > )
{
    l2cap_input({
        0x06, 0x01, 0x00, 0xFF, 0xFF, 0x00, 0x28,
        0xA9, 0x3C, 0xC7, 0x5B,     // Service UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    });

    static const std::uint8_t expected_response[] = {
        0x07,
        0x01, 0x00,
        0x07, 0x00
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_response ), std::end( expected_response ) );
}

typedef bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( test::temperature_value ), &test::temperature_value >,
            bluetoe::no_write_access
        >
    >,
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( test::temperature_value ), &test::temperature_value >,
            bluetoe::no_write_access
        >
    >,
    bluetoe::service<
        bluetoe::service_uuid16< 0xFF45 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( test::temperature_value ), &test::temperature_value >,
            bluetoe::no_write_access
        >
    >
> server_with_multiple_services;

BOOST_FIXTURE_TEST_CASE( discover_multiple_primary_service_by_service_uuid, test::request_with_reponse< server_with_multiple_services > )
{
    l2cap_input({
        0x06, 0x01, 0x00, 0xFF, 0xFF, 0x00, 0x28,
        0xAA, 0x3C, 0xC7, 0x5B,     // Service UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    });

    static const std::uint8_t expected_response[] = {
        0x07,
        0x01, 0x00,
        0x03, 0x00,
        0x04, 0x00,
        0x06, 0x00
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_response ), std::end( expected_response ) );
}

BOOST_FIXTURE_TEST_CASE( discover_multiple_primary_service_by_16_bit_service_uuid, test::request_with_reponse< server_with_multiple_services > )
{
    l2cap_input({
        0x06, 0x01, 0x00, 0xFF, 0xFF, 0x00, 0x28,
        0x45, 0xFF // Service UUID
    });

    static const std::uint8_t expected_response[] = {
        0x07,
        0x07, 0x00,
        0x09, 0x00
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_response ), std::end( expected_response ) );
}


typedef bluetoe::service<
    bluetoe::service_uuid16< 0xFF45 >,
    bluetoe::characteristic<
        bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
        bluetoe::bind_characteristic_value< decltype( test::temperature_value ), &test::temperature_value >,
        bluetoe::no_write_access
    >
> single_service;

typedef bluetoe::server<
    single_service,
    single_service,
    single_service,
    single_service,
    single_service,
    single_service,
    single_service,
    single_service,
    single_service,
    single_service,
    single_service,
    single_service,
    single_service,
    single_service,
    single_service,
    single_service,
    single_service,
    single_service,
    single_service,
    single_service
> server_with_20_services;

BOOST_FIXTURE_TEST_CASE( discover_multiple_primary_service_by_16_bit_service_uuid_till_buffer_is_full, test::request_with_reponse< server_with_20_services > )
{
    l2cap_input({
        0x06, 0x01, 0x00, 0xFF, 0xFF, 0x00, 0x28,
        0x45, 0xFF // Service UUID
    });

    static const std::uint8_t expected_response[] = {
        0x07,
        0x01, 0x00,
        0x03, 0x00,
        0x04, 0x00,
        0x06, 0x00,
        0x07, 0x00,
        0x09, 0x00,
        0x0A, 0x00,
        0x0C, 0x00,
        0x0D, 0x00,
        0x0F, 0x00,
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_response ), std::end( expected_response ) );
}

using server_with_fixed_handles = bluetoe::server<
    bluetoe::service<
        bluetoe::attribute_handle< 0x100 >,
        bluetoe::service_uuid16< 0xFF45 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::fixed_uint8_value< 0x42 >
        >
    >,
    bluetoe::service<
        bluetoe::service_uuid16< 0xFF45 >,
        bluetoe::characteristic<
            bluetoe::attribute_handles< 0x200, 0x220 >,
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::fixed_uint8_value< 0x42 >
        >
    >,
    bluetoe::service<
        bluetoe::service_uuid16< 0xFF45 >,
        bluetoe::attribute_handle< 0x2244 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::fixed_uint8_value< 0x42 >
        >
    >
>;

BOOST_FIXTURE_TEST_CASE( find_with_fixed_attribute_handles, test::request_with_reponse< server_with_fixed_handles > )
{
    l2cap_input({
        0x06,       // Find By Type Value Request
        0x01, 0x00, // First requested handle number
        0xFF, 0xFF, // Last requested handle number
        0x00, 0x28, // 2 octet UUID to find
        0x45, 0xFF  // Service UUID
    });

    expected_result( {
        0x07,
        0x00, 0x01,     // 0x100 -
        0x02, 0x01,     // 0x102
        0x03, 0x01,     // 0x103 -
        0x20, 0x02,     // 0x220
        0x44, 0x22,     // 0x2244 -
        0x46, 0x22      // 0x2246
    } );
}

BOOST_FIXTURE_TEST_CASE( find_in_gap, test::request_with_reponse< server_with_fixed_handles > )
{
    l2cap_input({
        0x06,       // Find By Type Value Request
        0x00, 0x03, // First requested handle number
        0x00, 0x04, // Last requested handle number
        0x00, 0x28, // 2 octet UUID to find
        0x45, 0xFF  // Service UUID
    });

    expected_result( {
        0x01,           // Error Response
        0x06,           // The request that generated this error response
        0x00, 0x03,     // The attribute handle that generated this error response
        0x0A            // Attribute Not Found
    } );
}

BOOST_FIXTURE_TEST_CASE( find_exactely_two_groups, test::request_with_reponse< server_with_fixed_handles > )
{
    l2cap_input({
        0x06,       // Find By Type Value Request
        0x03, 0x01, // First requested handle number
        0x44, 0x22, // Last requested handle number
        0x00, 0x28, // 2 octet UUID to find
        0x45, 0xFF  // Service UUID
    });

    expected_result( {
        0x07,
        0x03, 0x01,     // 0x103 -
        0x20, 0x02,     // 0x220
        0x44, 0x22,     // 0x2244 -
        0x46, 0x22      // 0x2246
    } );
}

BOOST_FIXTURE_TEST_CASE( find_exactely_not_the_first_and_last_service, test::request_with_reponse< server_with_fixed_handles > )
{
    l2cap_input({
        0x06,       // Find By Type Value Request
        0x03, 0x01, // First requested handle number
        0x43, 0x22, // Last requested handle number
        0x00, 0x28, // 2 octet UUID to find
        0x45, 0xFF  // Service UUID
    });

    expected_result( {
        0x07,
        0x03, 0x01,     // 0x103 -
        0x20, 0x02      // 0x220
    } );
}

BOOST_AUTO_TEST_SUITE_END()
